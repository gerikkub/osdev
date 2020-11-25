
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <kernel/messages.h>
#include <kernel/assert.h>
#include <kernel/task.h>
#include <kernel/kernelspace.h>
#include <kernel/vmem.h>
#include <kernel/bitutils.h>
#include <kernel/syscall.h>


typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t flags;
    uint16_t dst_id;
    uint16_t port_id;
    uint16_t src_id;
} msg_header;


void msg_queue_init(msg_queue* queue) {
    ASSERT(queue != NULL);

    queue->read_idx = 0;
    queue->write_idx = 0;
    queue->size = 0;
}

static bool msg_queue_push(msg_queue* queue, msg_placeholder* msg) {
    ASSERT(queue != NULL);
    ASSERT(queue->read_idx < MSG_QUEUE_MAXSIZE);
    ASSERT(queue->write_idx < MSG_QUEUE_MAXSIZE);

    uint64_t read_next = (queue->read_idx + 1) % MSG_QUEUE_MAXSIZE;
    if (read_next == queue->write_idx) {
        // No Space
        return false;
    }
    ASSERT(queue->size < MSG_QUEUE_MAXSIZE);

    queue->buffer[queue->write_idx] = *msg;
    queue->write_idx = (queue->write_idx + 1) % MSG_QUEUE_MAXSIZE;
    queue->size++;

    MEM_DMB();
    return true;
}

static bool msg_queue_pop(msg_queue* queue, msg_placeholder* msg_out) {
    ASSERT(queue != NULL);
    ASSERT(queue->read_idx < MSG_QUEUE_MAXSIZE);
    ASSERT(queue->write_idx < MSG_QUEUE_MAXSIZE);

    ASSERT(msg_out != NULL);

    if (queue->read_idx == queue->write_idx) {
        // No data
        return false;
    }
    ASSERT(queue->size > 0);

    *msg_out = queue->buffer[queue->read_idx];

    queue->read_idx = (queue->read_idx + 1) % MSG_QUEUE_MAXSIZE;
    queue->size--;

    MEM_DMB();

    return true;
}

static uint64_t msg_queue_size(msg_queue* queue) {
    ASSERT(queue != NULL);
    return queue->size;
}

static int64_t wakeup_getmsgs(task_t* task) {
    return syscall_getmsgs(task->reg.gp[TASK_REG(0)],
                           task->reg.gp[TASK_REG(1)],
                           task->reg.gp[TASK_REG(2)],
                           task->reg.gp[TASK_REG(3)]);
}

static bool canwakeup_getmsgs(wait_ctx_t* wait_ctx) {
    ASSERT(wait_ctx);

    msg_queue* wait_queue = wait_ctx->getmsgs.wait_queue;

    return msg_queue_size(wait_queue) > 0;
}

int64_t syscall_getmsgs(uint64_t msg_buffer,
                         uint64_t msg_buffer_size,
                         uint64_t x2_dummy,
                         uint64_t x3_dummy) {

    if (msg_buffer_size < sizeof(msg_placeholder)) {
        return SYSCALL_ERROR_BADARG;
    }

    task_t* active_task = get_active_task();

    uint64_t phy_addr;
    bool res = vmem_walk_table(active_task->low_vm_table, msg_buffer, &phy_addr);
    if (!res) {
        return SYSCALL_ERROR_BADARG;
    }

    msg_queue* msgs = &active_task->msgs;

    if (msg_queue_size(msgs) == 0) {
        wait_ctx_t wait_ctx;
        wait_getmsgs_t wait_msg;
        wait_msg.wait_queue = msgs;
        wait_ctx.getmsgs = wait_msg;
        task_wait(active_task, WAIT_GETMSGS, wait_ctx, wakeup_getmsgs);
    }

    msg_placeholder msg;
    bool have_msg = msg_queue_pop(msgs, &msg);
    ASSERT(have_msg);

    // Pointer to msg_buffer in memory mapped in kernel space
    msg_placeholder* msg_buffer_kmem = (msg_placeholder*)PHY_TO_KSPACE(phy_addr);

    *msg_buffer_kmem = msg;

    return 1;
}

int64_t syscall_sendmsg(uint64_t msg_0,
                        uint64_t msg_1,
                        uint64_t msg_2,
                        uint64_t msg_3) {

    msg_placeholder msg = {
        .msg = {msg_0, msg_1, msg_2, msg_3}
    };

    msg_header header;
    memcpy(&header, &msg_0, sizeof(msg_0));

    if (header.dst_id >= MSG_MAX_DSTS) {
        return SYSCALL_ERROR_BADARG;
    }

    uint32_t dst_tid = header.dst_id;
    if (dst_tid == 0) {
        return SYSCALL_ERROR_BADARG;
    }
    
    task_t* dst_task = get_task_for_tid(dst_tid);
    if (dst_task == NULL) {
        return SYSCALL_ERROR_NORESOURCE;
    }

    bool res;
    res = msg_queue_push(&dst_task->msgs, &msg);
    if (!res) {
        return SYSCALL_ERROR_NOSPACE;
    }

    task_wakeup(dst_task, WAIT_GETMSGS, canwakeup_getmsgs);

    return SYSCALL_ERROR_OK;
}