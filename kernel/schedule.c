
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "kernel/task.h"
#include "kernel/assert.h"
#include "kernel/vmem.h"
#include "kernel/exception.h"
#include "kernel/memoryspace.h"
#include "kernel/kernelspace.h"
#include "kernel/console.h"
#include "kernel/gtimer.h"
#include "kernel/lib/llist.h"
#include "kernel/lib/lstruct.h"
#include "kernel/lib/vmalloc.h"

#include "kernel/interrupt/interrupt.h"

#define TASKQ_FOREACH(head, ptr) FOREACH_LSTRUCT(head, ptr, schedule_queue)
#define TASKQ_AT(head, idx) LSTRUCT_AT(head, idx, task_t, schedule_queue)

typedef struct {
    lstruct_head_t proc_runable;
    lstruct_head_t proc_wait_wakeup;
    lstruct_head_t proc_wait;
    lstruct_head_t proc_complete;

    task_t* active_task;
} schedule_ctx_t;

schedule_ctx_t s_schedule_ctx;

task_t* get_active_task(void) {
    return s_schedule_ctx.active_task;
}

void schedule_init(void) {
    lstruct_init_head(&s_schedule_ctx.proc_runable);
    lstruct_init_head(&s_schedule_ctx.proc_wait_wakeup);
    lstruct_init_head(&s_schedule_ctx.proc_wait);
    lstruct_init_head(&s_schedule_ctx.proc_complete);
}

static void wait_timer_setup(uint64_t now_us, uint64_t max_sleep_time);

void task_requeue(task_t* task) {
    if (task->schedule_queue.p != NULL) {
        lstruct_remove(&task->schedule_queue);
    }

    switch (task->run_state) {
        case TASK_RUNABLE:
        case TASK_AWAKE:
            lstruct_prepend(s_schedule_ctx.proc_runable, &task->schedule_queue);
            break;
        case TASK_WAIT:
            lstruct_prepend(s_schedule_ctx.proc_wait, &task->schedule_queue);
            break;
        case TASK_WAIT_WAKEUP:
            lstruct_prepend(s_schedule_ctx.proc_wait_wakeup, &task->schedule_queue);
            break;
        case TASK_COMPLETE:
            lstruct_prepend(s_schedule_ctx.proc_complete, &task->schedule_queue);
            break;
        default:
            ASSERT(0);
    }
}

void schedule(void) {

    DISABLE_IRQ();

    uint32_t* gpio_set = (uint32_t*)(0xffff00047e200000 + 0x1c);
    uint32_t* gpio_clr = (uint32_t*)(0xffff00047e200000 + 0x28);
    *gpio_set = (1 << 17);

    task_t* active_task = get_active_task();

    if (active_task != NULL) {
        elapsedtimer_stop(&active_task->profile_time);
    }

    //while (gic_try_irq_handler());

    start_idle_timer();

    uint64_t now_us = gtimer_get_count_us();

    // Attempted to wakeup all tasks
    task_t* wait_task;
    TASKQ_FOREACH(s_schedule_ctx.proc_wait_wakeup, wait_task) {
        ASSERT(wait_task->run_state == TASK_WAIT_WAKEUP);

        int64_t task_ret;
        bool awoke = wait_task->wait_wakeup_fn(wait_task, &task_ret);
        if (awoke) {
            lstruct_remove(&wait_task->schedule_queue);
            lstruct_prepend(s_schedule_ctx.proc_runable, &wait_task->schedule_queue);
            wait_task->run_state = TASK_AWAKE;
            wait_task->wait_return = task_ret;
        }
    }

    task_t* run_task = TASKQ_AT(s_schedule_ctx.proc_runable, 0);

    wait_timer_setup(now_us, TASK_MAX_PROC_TIME_US);
    (void)now_us;

    *gpio_clr = (1 << 17);

    if (run_task != NULL) {
        s_schedule_ctx.active_task = run_task;

        lstruct_remove(&run_task->schedule_queue);
        lstruct_append(s_schedule_ctx.proc_runable, &run_task->schedule_queue);

        stop_idle_timer();
        elapsedtimer_start(&run_task->profile_time);

        switch (run_task->run_state) {
            case TASK_RUNABLE:
                restore_context(run_task->tid);
                break;
            case TASK_AWAKE:
                run_task->run_state = TASK_RUNABLE;
                restore_context_kernel(run_task->tid, run_task->wait_return, run_task->kernel_wait_sp);
                break;
            default:
                ASSERT(0);
                break;
        }
    } else {
        // Schedule idle task
        s_schedule_ctx.active_task = NULL;
        restore_context_idle();
    }

    ASSERT(0);
}

static void wait_timer_setup(uint64_t now_us, uint64_t max_sleep_time) {

    uint64_t timer_fire_time = now_us + max_sleep_time;

    task_t* check_task = NULL;
    TASKQ_FOREACH(s_schedule_ctx.proc_wait, check_task) {
        ASSERT (check_task->run_state == TASK_WAIT)

        uint64_t wake_time_us;

        switch (check_task->wait_reason) {
        case WAIT_TIMER:
            wake_time_us = check_task->wait_ctx.timer.wake_time_us;
            break;
        case WAIT_SELECT:
            if (!check_task->wait_ctx.select.timeout_valid) {
                continue;
            }
            wake_time_us = check_task->wait_ctx.select.timeout_end_us;
            break;
        default:
            continue;
        }

        if (wake_time_us <= now_us) {
            check_task->run_state = TASK_WAIT_WAKEUP;
            task_requeue(check_task);
            continue;
        }

        if (timer_fire_time == 0) {
            timer_fire_time = wake_time_us;
        } else if (timer_fire_time > wake_time_us) {
            timer_fire_time = wake_time_us;
        }
    }

    TASKQ_FOREACH(s_schedule_ctx.proc_wait_wakeup, check_task) {

        ASSERT (check_task->run_state == TASK_WAIT_WAKEUP);

        uint64_t wake_time_us;

        switch (check_task->wait_reason) {
        case WAIT_SELECT:
            if (!check_task->wait_ctx.select.timeout_valid) {
                continue;
            }
            wake_time_us = check_task->wait_ctx.select.timeout_end_us;
            break;
        default:
            continue;
        }

        if (wake_time_us <= now_us) {
            continue;
        }

        if (timer_fire_time == 0) {
            timer_fire_time = wake_time_us;
        } else if (timer_fire_time > wake_time_us) {
            timer_fire_time = wake_time_us;
        }
    }

    uint64_t downcount;
    READ_SYS_REG(CNTP_TVAL_EL0, downcount);

    if (now_us >= timer_fire_time) {
        timer_fire_time = now_us + 1;
    }

    gtimer_start_downtimer_us(timer_fire_time - now_us, true);
}
