
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <kernel/messages.h>
#include <kernel/kmalloc.h>
#include <kernel/assert.h>
#include <kernel/task.h>
#include <kernel/kernelspace.h>
#include <kernel/vmem.h>
#include <stdlib/bitutils.h>
#include <kernel/syscall.h>



void msg_queue_init(msg_queue* queue) {
    ASSERT(queue != NULL);

    queue->read_idx = 0;
    queue->write_idx = 0;
    queue->size = 0;
}

static bool msg_queue_push(msg_queue* queue, msg_placeholder_t* msg) {
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

static bool msg_queue_pop(msg_queue* queue, msg_placeholder_t* msg_out) {
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

static int64_t msg_translate_pointers(system_msg_memory_t* msg, task_t* src_task, task_t* dst_task) {
    ASSERT(msg != NULL);
    ASSERT(src_task != NULL);
    ASSERT(dst_task != NULL);

    // Get the physical address of the pointer data in
    // the source address space
    uint64_t src_phy_addr;
    bool walk_ok;
    walk_ok = vmem_walk_table(src_task->low_vm_table, msg->ptr, &src_phy_addr);
    if (!walk_ok) {
        return SYSCALL_ERROR_BADARG;
    }

    // Allocate physical memory for the message
    uint8_t* dst_phy_ptr = kmalloc_phy(msg->len);
    ASSERT(dst_phy_ptr != NULL);

    // Copy pointer data from the source to destination physical memory
    memcpy((void*)PHY_TO_KSPACE(dst_phy_ptr), (void*)PHY_TO_KSPACE(src_phy_addr), msg->len);

    // Allocate virtual memory space in the destination space
    memory_entry_phy_t dst_phy_entry;
    bool alloc_ok;
    alloc_ok = memspace_alloc_space(&dst_task->memory, msg->len, (memory_entry_t*)&dst_phy_entry);
    if (!alloc_ok) {
        return SYSCALL_ERROR_NOSPACE;
    }

    // Point the allocated virtual memory to the allocated physical memory
    dst_phy_entry.type = MEMSPACE_PHY;
    dst_phy_entry.flags = MEMSPACE_FLAG_PERM_URW;
    dst_phy_entry.phy_addr = (uintptr_t)dst_phy_ptr;
    dst_phy_entry.kmem_addr = PHY_TO_KSPACE(dst_phy_ptr);

    // Add the entry to the destination memory space
    bool add_ok;
    add_ok = memspace_add_entry_to_memory(&dst_task->memory, (memory_entry_t*)&dst_phy_entry);
    if (!add_ok) {
        kfree_phy(dst_phy_ptr);
        return SYSCALL_ERROR_NOSPACE;
    }

    // Rebuild the virtual address space
    _vmem_table* new_dst_vmem = memspace_build_vmem(&dst_task->memory);
    ASSERT(new_dst_vmem);
    dst_task->low_vm_table = new_dst_vmem;

    // Adjust the pointer in the msg to the destination memory
    msg->ptr = dst_phy_entry.start;

    return SYSCALL_ERROR_OK;
}

static int64_t wakeup_getmsgs(task_t* task) {
    return syscall_getmsgs(task->reg.gp[TASK_REG(0)],
                           task->reg.gp[TASK_REG(1)],
                           task->reg.gp[TASK_REG(2)],
                           task->reg.gp[TASK_REG(3)]);
}

static bool canwakeup_getmsgs(wait_ctx_t* wait_ctx, void* ctx) {
    ASSERT(wait_ctx);

    msg_queue* wait_queue = wait_ctx->getmsgs.wait_queue;

    return msg_queue_size(wait_queue) > 0;
}

int64_t syscall_getmsgs(uint64_t msg_buffer,
                        uint64_t msg_buffer_size,
                        uint64_t msg_free_list,
                        uint64_t msg_free_list_size) {

    if (msg_buffer_size < sizeof(msg_placeholder_t)) {
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

    msg_placeholder_t msg;
    bool have_msg = msg_queue_pop(msgs, &msg);
    ASSERT(have_msg);

    // Pointer to msg_buffer in memory mapped in kernel space
    msg_placeholder_t* msg_buffer_kmem = (msg_placeholder_t*)PHY_TO_KSPACE(phy_addr);

    *msg_buffer_kmem = msg;

    return 1;
}

int64_t syscall_sendmsg(uint64_t msg_0,
                        uint64_t msg_1,
                        uint64_t msg_2,
                        uint64_t msg_3) {

    msg_placeholder_t msg = {
        .msg = {msg_0, msg_1, msg_2, msg_3}
    };

    msg_header_t header;
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

    // Confirm the message is valid. Perform any type
    // dependent fixes if necessary
    int64_t translate_ok;
    switch (header.type) {
        case MSG_TYPE_PAYLOAD:
            // No packet fixes necessary
            break;
        case MSG_TYPE_MEMORY:
            translate_ok = msg_translate_pointers((system_msg_memory_t*)&msg, get_active_task(), dst_task);
            if (translate_ok < 0) {
                return translate_ok;
            }
            break;
        default:
            return SYSCALL_ERROR_BADARG;
    }

    bool res;
    res = msg_queue_push(&dst_task->msgs, &msg);
    if (!res) {
        return SYSCALL_ERROR_NOSPACE;
    }

    task_wakeup(dst_task, WAIT_GETMSGS, canwakeup_getmsgs, NULL);

    return SYSCALL_ERROR_OK;
}