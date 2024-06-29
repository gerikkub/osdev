
#include <stdint.h>
#include <stdbool.h>

#include "kernel/syscall.h"
#include "kernel/task.h"
#include "kernel/kernelspace.h"
#include "kernel/fd.h"
#include "kernel/assert.h"
#include "kernel/console.h"
#include "kernel/gtimer.h"
#include "kernel/select.h"

#define SELECT_VOLATILE_BITS (0xFFFFFFFF00000000ULL)

int64_t select_create_simple_waiter(task_t* task) {
    int64_t fd = find_open_fd(task);

    if (fd < 0) {
        return fd;
    }
    fd_ctx_t* fd_ctx = get_task_fd(fd, task);
    ASSERT(fd_ctx != NULL);

    fd_ctx->ctx = NULL;
    fd_ctx->ops.read = NULL;
    fd_ctx->ops.write = NULL;
    fd_ctx->ops.ioctl = NULL;
    fd_ctx->ops.close = NULL;
    fd_ctx->ready = 0;
    fd_ctx->task = task;
    fd_ctx->valid = true;

    return fd;
}

int64_t poll_select(task_t* task, syscall_select_ctx_t* select_arr, uint64_t select_len, uint64_t* ready_mask_out) {
    
    for (uint64_t idx = 0; idx < select_len; idx++) {
        if (select_arr[idx].fd < 0) {
            continue;
        }

        ASSERT(select_arr[idx].fd < MAX_TASK_FDS);

        fd_ctx_t* fd_ctx = get_task_fd(select_arr[idx].fd, task);
        ASSERT(fd_ctx->valid);

        if (fd_ctx->ready & select_arr[idx].ready_mask) {
            if (ready_mask_out != NULL) {
                *ready_mask_out = fd_ctx->ready & select_arr[idx].ready_mask;
            }
            return select_arr[idx].fd;
        }
    }

    return -1;
}

void select_task_wakeup(task_t* task) {
    task_wakeup(task, WAIT_SELECT);
}

static bool select_wakeup(task_t* task, bool timeout, int64_t* ret) {
    uint64_t ready_bits;
    int64_t fd = poll_select(task->wait_ctx.select.task,
                             task->wait_ctx.select.select_arr,
                             task->wait_ctx.select.select_len,
                             &ready_bits);
    
    if (fd < 0) {
        *ret = -1;
        return timeout;
    }
    
    fd_ctx_t* fd_ctx = get_task_fd(fd, task);
    ASSERT(fd_ctx->valid);

    fd_ctx->ready = fd_ctx->ready ^ (SELECT_VOLATILE_BITS & ready_bits);

    if (task->wait_ctx.select.ready_mask_out != NULL) {
        *task->wait_ctx.select.ready_mask_out = ready_bits;
    }

    *ret = fd;
    return true;
}


int64_t syscall_select(uint64_t select_arr_ptr, uint64_t select_len, uint64_t timeout_us, uint64_t ready_mask_ptr) {
    
    task_t* task = get_active_task();

    syscall_select_ctx_t* select_arr_kptr = get_userspace_ptr(task->low_vm_table, select_arr_ptr);

    if (select_arr_kptr == NULL) {
        return -1;
    }

    uint64_t* ready_mask_out = NULL;
    if (ready_mask_ptr != 0) {
        ready_mask_out = get_userspace_ptr(task->low_vm_table, ready_mask_ptr);
    }

    return select_wait(select_arr_kptr, select_len, timeout_us, ready_mask_out);
}

int64_t select_wait(syscall_select_ctx_t* select_arr, uint64_t select_len, uint64_t timeout_us, uint64_t* ready_mask_out) {

    task_t* task = get_active_task();

    uint64_t ready_bits;
    int64_t select_stat = poll_select(task, select_arr, select_len, &ready_bits);

    if (select_stat >= 0) {

        fd_ctx_t* fd_ctx = get_task_fd(select_stat, task);
        ASSERT(fd_ctx->valid);

        fd_ctx->ready = fd_ctx->ready ^ (SELECT_VOLATILE_BITS & ready_bits);

        if (ready_mask_out != NULL) {
            *ready_mask_out = ready_bits;
        }
        return select_stat;
    }

    if (timeout_us == 0) {
        return -1;
    }

    wait_ctx_t wait_ctx = {
        .select = {
            .task = task,
            .select_arr = select_arr,
            .select_len = select_len,
            .ready_mask_out = ready_mask_out
        },
        .wake_at = timeout_us != UINT64_MAX ? gtimer_get_count_us() + timeout_us :
                                              0
    };

    return task_wait_kernel(task, WAIT_SELECT, &wait_ctx, TASK_WAIT_WAKEUP, select_wakeup);
}
