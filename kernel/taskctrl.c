
#include <stdint.h>
#include <stdbool.h>

#include "kernel/task.h"
#include "kernel/fd.h"
#include "kernel/lib/vmalloc.h"

#include "k_ioctl_common.h"
#include "k_select.h"

typedef struct {
    uint64_t tid;
    bool tid_valid;
    fd_ctx_t* fd_ctx;
} task_ops_ctx_t;

uint64_t task_ops_calculate_ready(task_t* target_task) {
    uint64_t ready_mask = 0;

    if (target_task->run_state == TASK_COMPLETE) {
        ready_mask |= FD_READY_TASKOPS_WAIT;
    }

    return ready_mask;
}

void task_ops_tid_validate(task_ops_ctx_t* task_ctx) {
    task_t* target_task = get_task_for_tid(task_ctx->tid);

    if (target_task == NULL) {
        task_ctx->tid_valid = false;
        task_ctx->fd_ctx->ready = FD_READY_GEN_CLOSE;
    }
}

void task_ops_waited(task_t* task, task_t* target_task, void* ctx) {
    task_ops_ctx_t* task_ctx = ctx;
    task_ctx->fd_ctx->ready = task_ops_calculate_ready(target_task);
}

bool task_ops_wait_canwakeup_fn(wait_ctx_t* wait_ctx) {
    return wait_ctx->wait.complete;
}

int64_t task_ops_wait_wakeup_fn(task_t* task) {
    task_ops_ctx_t* task_ctx = task->wait_ctx.wait.ctx;
    return task_await(task, get_task_for_tid(task_ctx->tid));
}

int64_t task_ops_ioctl_fn(void* ctx, const uint64_t ioctl, const uint64_t* args, const uint64_t arg_count) {
    task_ops_ctx_t* task_ctx = ctx;

    task_ops_tid_validate(task_ctx);

    if (!task_ctx->tid_valid) {
        return -1;
    }

    task_t* task = get_active_task();
    task_t* target_task = get_task_for_tid(task_ctx->tid);
    int64_t ret_val = 0;

    switch (ioctl) {
    case TASKCTRL_IOCTL_WAIT:
        if (target_task->run_state == TASK_COMPLETE) {
            ret_val = task_await(task, target_task);
        } else {

            wait_ctx_t wait_ctx = {
                .wait.task = task,
                .wait.ctx = task_ctx,
                .wait.complete = false
            };
            ret_val = task_wait_kernel(task,
                                       WAIT_WAIT,
                                       &wait_ctx,
                                       task_ops_wait_wakeup_fn,
                                       task_ops_wait_canwakeup_fn);
        }
        break;
    default:
        ret_val = -1;
        break;
    }

    task_ops_tid_validate(task_ctx);

    task_ctx->fd_ctx->ready = task_ops_calculate_ready(target_task);

    return ret_val;
}

int64_t task_ops_close_fn(void* ctx) {

    task_ops_ctx_t* task_ctx = ctx;

    if (task_ctx->tid_valid) {
        task_t* task = get_active_task();
        task_t* target_task = get_task_for_tid(task_ctx->tid);

        task_clear_waiter(task, target_task);
    }

    vfree(ctx);

    return 0;
}

static fd_ops_t s_task_fd_ops = {
    .read = NULL,
    .write = NULL,
    .ioctl = task_ops_ioctl_fn,
    .close = task_ops_close_fn,
};

int64_t syscall_taskctrl(uint64_t tid, uint64_t x1, uint64_t x2, uint64_t x3) {

    task_t* task = get_active_task();
    task_t* target_task = get_task_for_tid(tid);

    if (target_task == NULL) {
        return -1;
    }

    int64_t fd_num = find_open_fd(task);
    if (fd_num < 0) {
        return -1;
    }

    task_ops_ctx_t* ctx = vmalloc(sizeof(task_ops_ctx_t));

    ctx->tid = tid;
    ctx->fd_ctx = &task->fds[fd_num];
    ctx->tid_valid = true;
    
    task->fds[fd_num].ctx = ctx;
    task->fds[fd_num].ops = s_task_fd_ops;
    task->fds[fd_num].valid = true;
    task->fds[fd_num].ready = task_ops_calculate_ready(target_task);

    task_add_waiter(task, target_task);

    return fd_num;
}
