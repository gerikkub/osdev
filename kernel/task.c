

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "kernel/task.h"
#include "kernel/assert.h"
#include "kernel/kmalloc.h"
#include "kernel/vmem.h"
#include "kernel/exception.h"
#include "kernel/memoryspace.h"
#include "kernel/kernelspace.h"
#include "kernel/console.h"
#include "kernel/gtimer.h"

#include "kernel/interrupt/interrupt.h"

bool gic_try_irq_handler();

static task_t s_task_list[MAX_NUM_TASKS] = {0};

static volatile uint64_t s_active_task_idx;

static uint64_t s_max_tid;

static _vmem_table* s_dummy_user_table;

static elapsedtimer_t s_idletime;

static uint64_t* s_exstack;

extern uint8_t _stack_base;

void switch_to_kernel_task_stack_asm(uint64_t vector,
                                     uint64_t cpu_sp,
                                     uint64_t task_sp,
                                     exception_handler func);


void task_init(uint64_t* exstack) {
    ASSERT(exstack != NULL);

    s_max_tid = 1;
    s_dummy_user_table = vmem_allocate_empty_table();
    s_exstack = exstack;

    elapsedtimer_clear(&s_idletime);
}

task_t* get_active_task(void) {
    ASSERT(s_active_task_idx < MAX_NUM_TASKS);

    return &s_task_list[s_active_task_idx];
}

task_t* get_task_at_idx(int64_t idx) {
    if (idx >= MAX_NUM_TASKS) {
        return NULL;
    }

    if (s_task_list[idx].tid == 0) {
        return NULL;
    }

    return &s_task_list[idx];
}

uint64_t get_schedule_profile_time(void) {
    return elapsedtimer_get_us(&s_idletime);
}

void bad_task_return(void) {
    ASSERT(0);
}

task_t* get_task_for_tid(uint64_t tid) {

    uint64_t idx;
    for (idx = 0; idx < MAX_NUM_TASKS; idx++) {
        if (tid == s_task_list[idx].tid) {
            break;
        }
    }

    if (idx != MAX_NUM_TASKS) {
        return &s_task_list[idx];
    } else {
        return NULL;
    }
}

void save_context(task_reg_t* state_fp,
                  uint64_t vector,
                  exception_handler func) {

    ASSERT(state_fp != NULL);
    ASSERT(func != NULL);

    uint64_t task_tid;
    READ_SYS_REG(TPIDR_EL0, task_tid);

    task_t* task = &s_task_list[s_active_task_idx];
    ASSERT(task_tid == task->tid);

    task->reg = *state_fp;

    task->run_state = TASK_RUNABLE;

    void* cpu_stack_base = &_stack_base;

    switch_to_kernel_task_stack_asm(vector,
                                    (uint64_t)cpu_stack_base,
                                    (uint64_t)task->kernel_stack_base,
                                    func);

    ASSERT(1); // Should not reach
}

void restore_context(uint64_t tid) {

    task_t* task = get_task_for_tid(tid);

    WRITE_SYS_REG(TPIDR_EL0, tid);

    uint64_t sp0t = task->reg.sp;
    uint64_t cpu_stack_base = (uint64_t)&_stack_base;
    uint64_t currSP = 0;
    READ_SYS_REG(SPSel, currSP);

    vmem_set_user_table((_vmem_table*)KSPACE_TO_PHY(task->low_vm_table), tid);

    restore_context_asm(&task->reg, sp0t, cpu_stack_base, currSP);

    ASSERT(1); // Should not reach
}

void idle_task(void) {

    while (true) {
        asm ("wfi");
        schedule();
    }
}

void restore_context_idle(void) {

    uint64_t tid = 0;
    WRITE_SYS_REG(TPIDR_EL0, tid);

    uint64_t sp0t = (uint64_t)&_stack_base;
    uint64_t spsr = TASK_SPSR_M(4);
    task_reg_t reg = {
        .spsr = spsr,
        .sp = sp0t,
        .elr = (uint64_t)idle_task,
        .gp = {0}
    };

    uint64_t currSP = 0;
    READ_SYS_REG(SPSel, currSP);

    vmem_set_user_table(KSPACE_TO_PHY_PTR(s_dummy_user_table), 0);

    restore_context_asm(&reg, sp0t, sp0t, currSP);

    ASSERT(1);

}

void restore_context_kernel(uint64_t tid, uint64_t x0, void* sp) {

    task_t* task = get_task_for_tid(tid);

    WRITE_SYS_REG(TPIDR_EL0, tid);

    if (task->low_vm_table != NULL) {
        vmem_set_user_table((_vmem_table*)KSPACE_TO_PHY(task->low_vm_table), tid);
    }

    restore_context_kernel_asm(x0, task->kernel_wait_sp);

    ASSERT(1); // Should not reach
}

bool create_task_canwakeup_f(wait_ctx_t* wait_ctx) {
    // Dummy wait for new tasks
    return true;
}

int64_t create_task_wakeup_f(task_t* task) {
    return task->wait_ctx.init_thread.x0;
}

uint64_t create_task(uint64_t* user_stack_base,
                     uint64_t user_stack_size,
                     uint64_t* kernel_stack_base,
                     uint64_t kernel_stack_size,
                     task_reg_t* reg,
                     memory_space_t* memspace,
                     bool kernel_task,
                     const char* name) {

    ASSERT(kernel_stack_base != NULL);
    ASSERT(kernel_stack_size > 0);
    ASSERT(user_stack_base != NULL || kernel_task);
    ASSERT((void*)reg->elr != NULL);

    // Allocate an id
    uint64_t tid = s_max_tid + 1;
    s_max_tid++;

    if (kernel_task) {
        tid |= TASK_TID_KERNEL;
    }

    // Find an empty task entry
    uint64_t idx;
    for (idx = 0; idx < MAX_NUM_TASKS; idx++) {
        if (s_task_list[idx].tid == 0) {
            break;
        }
    }
    ASSERT(idx < MAX_NUM_TASKS);

    task_t* task = &s_task_list[idx];

    task->tid = tid;
    task->asid = 0;
    elapsedtimer_clear(&task->profile_time);

    task->user_stack_base = user_stack_base;
    task->user_stack_size = user_stack_size;

    task->kernel_stack_base = kernel_stack_base;
    task->kernel_stack_size = kernel_stack_size;

    task->reg = *reg;

    task->ret_val = 0xFFFFFFFFDEADBEEF;
    task->waiters = llist_create();

    msg_queue_init(&task->msgs);

    if (memspace != NULL) {
        task->memory = *memspace;
        task->low_vm_table = memspace_build_vmem(memspace);
    } else {
        memspace_alloc(&task->memory, NULL);
        task->low_vm_table = s_dummy_user_table;
    }

    if (!kernel_task) {
        bool walk_ok;
        uintptr_t user_stack_base_kmem;
        walk_ok = vmem_walk_table(task->low_vm_table, (uint64_t)user_stack_base - 8, &user_stack_base_kmem);
        ASSERT(walk_ok);
        uint64_t* user_stack_base_kmem_ptr = (uint64_t*)(user_stack_base_kmem + 8);

        // Setup a stack frame for a malformed task
        user_stack_base_kmem_ptr[-1] = (uint64_t)bad_task_return;
        user_stack_base_kmem_ptr[-2] = (uint64_t)&user_stack_base_kmem_ptr[-2];

        task->reg.sp = (uint64_t)&user_stack_base[-2];
        task->reg.gp[TASK_REG_FP] = (uint64_t)&user_stack_base[-2];

        task->run_state = TASK_RUNABLE;
    } else {
        task->wait_canwakeup_fn = create_task_canwakeup_f;
        task->wait_wakeup_fn = create_task_wakeup_f;
        task->wait_ctx.init_thread.x0 = task->reg.gp[TASK_REG(1)];
        task->run_state = TASK_WAIT;

        // Set kernel registers to 0
        task->kernel_wait_sp = (void*)&kernel_stack_base[-28];
        memset(task->kernel_wait_sp, 0, 26 * sizeof(uint64_t));

        // Set link register to point at the start address
        ((uint64_t*)task->kernel_wait_sp)[24] = task->reg.elr;
        // Set frame pointer to bad kernel frame
        ((uint64_t*)task->kernel_wait_sp)[25] = (uint64_t)&kernel_stack_base[-2];
    }
    kernel_stack_base[-1] = (uint64_t)bad_task_return;
    kernel_stack_base[-2] = (uint64_t)&kernel_stack_base[-2];

    strncpy(task->name, name, MAX_TASK_NAME_LEN);

    for (int idx = 0; idx < MAX_TASK_FDS; idx++) {
        task->fds[idx].valid = false;
    }

    // Populate x0 with the task's tid
    task->reg.gp[TASK_REG(0)] = task->tid;

    return task->tid;
}

uint64_t create_kernel_task(uint64_t stack_size,
                            task_f task_entry,
                            void* ctx,
                            const char* name) {

    void* stack_ptr_phy = kmalloc_phy(stack_size);
    ASSERT(stack_ptr_phy != NULL);
    uint64_t* stack_ptr = (uint64_t*)PHY_TO_KSPACE(stack_ptr_phy);

    task_reg_t reg;
    reg.gp[TASK_REG(1)] = (uint64_t)ctx;
    reg.spsr = TASK_SPSR_M(4) | // EL1t SP
               TASK_SPSR_I |
               TASK_SPSR_F;
    reg.elr = (uint64_t)task_entry;

    return create_task(NULL, 0, ((void*)stack_ptr)+stack_size, stack_size, &reg, NULL, true, name);
}

uint64_t create_system_task(uint64_t kernel_stack_size,
                            uintptr_t user_stack_base,
                            uint64_t user_stack_size,
                            memory_space_t* memspace,
                            task_f task_entry,
                            void* ctx,
                            const char* name) {

    void* kernel_stack_ptr_phy = kmalloc_phy(kernel_stack_size);
    ASSERT(kernel_stack_ptr_phy != NULL);
    uint64_t* kernel_stack_ptr = (uint64_t*)PHY_TO_KSPACE(kernel_stack_ptr_phy);

    /*
    memory_entry_stack_t  kernel_stack_entry = {
        .start = (uintptr_t)kernel_stack_ptr,
        .end = PAGE_CEIL(((uintptr_t)kernel_stack_ptr) + kernel_stack_size),
        .type = MEMSPACE_PHY,
        .flags = MEMSPACE_FLAG_PERM_URW,
        .phy_addr = (uintptr_t)kernel_stack_ptr_phy,
        .base = (uintptr_t)kernel_stack_ptr,
        .limit = PAGE_CEIL(((uintptr_t)kernel_stack_ptr) + kernel_stack_size),
        .maxlimit = PAGE_CEIL(((uintptr_t)kernel_stack_ptr) + kernel_stack_size)
    };
    */

    /*
    bool memspace_result;
    memspace_result = memspace_add_entry_to_kernel_memory((memory_entry_t*)&kernel_stack_entry);
    if (!memspace_result) {
        kfree_phy(kernel_stack_ptr_phy);
        return 0;
    }
    */

    task_reg_t reg;
    for (uint64_t idx = 0; idx < 31; idx++) {
        reg.gp[TASK_REG(idx)] = 0;
    }
    reg.gp[TASK_REG(1)] = (uint64_t)ctx;
    reg.spsr = TASK_SPSR_M(0); // EL0t SP
    reg.elr = (uint64_t)task_entry;

    /*
    uint64_t tid = create_task((uint64_t*)user_stack_base, user_stack_size,
                       kernel_stack_ptr + kernel_stack_size, kernel_stack_size,
                       &reg, memspace, false, name);
    task_cleanup(get_task_for_tid(tid), 0);
    return 0; // Leak 0 bytes
    */

    return create_task((uint64_t*)user_stack_base, user_stack_size,
                       (uint64_t*)(PAGE_CEIL((uintptr_t)kernel_stack_ptr) + kernel_stack_size), kernel_stack_size,
                       &reg, memspace, false, name); // Leak 416-760 bytes
}

uint64_t create_user_task(uint64_t kernel_stack_size,
                          uintptr_t user_stack_base,
                          uint64_t user_stack_size,
                          memory_space_t* memspace,
                          task_f task_entry,
                          void* ctx,
                          const char* name) {

    return create_system_task(kernel_stack_size,
                              user_stack_base, 
                              user_stack_size,
                              memspace,
                              task_entry,
                              ctx, name);
}

/*
void task_wait(task_t* task, wait_reason_t reason, wait_ctx_t ctx, task_wakeup_f wakeup_fun) {
    ASSERT(task != NULL);

    task->run_state = TASK_WAIT;
    task->wait_reason = reason;
    task->wait_ctx = ctx;
    task->wait_wakeup_fun = wakeup_fun;

    schedule();
}
*/

void task_wait_kernel_c(task_t* task,
                        wait_reason_t reason,
                        wait_ctx_t* ctx,
                        task_wakeup_f wakeup_fn,
                        task_canwakeup_f canwakeup_fn,
                        void* kernel_sp) {
    ASSERT(task != NULL);

    task->run_state = TASK_WAIT;
    task->wait_reason = reason;
    task->wait_ctx = *ctx;
    task->wait_wakeup_fn = wakeup_fn;
    task->wait_canwakeup_fn = canwakeup_fn;
    task->kernel_wait_sp = kernel_sp;

    schedule();
}

/*
 * This may be called from an interrupt context
 */
void task_wakeup(task_t* task, wait_reason_t reason) {
    ASSERT(task != NULL);

    if ((task->run_state == TASK_RUNABLE) ||
        (task->wait_reason != reason)) {
        return;
    }

    if (task->wait_canwakeup_fn(&task->wait_ctx) || task->wait_canwakeup_fn == NULL) {
        task->run_state = TASK_AWAKE;
    }
}

void task_cleanup(task_t* task, int64_t ret_val) {

    int idx;
    for (idx = 0; idx < MAX_TASK_FDS; idx++) {
        if (task->fds[idx].valid &&
            task->fds[idx].ops.close != NULL) {

            task->fds[idx].ops.close(task->fds[idx].ctx);
        }
    }

    memory_entry_t* entry;
    FOR_LLIST(task->memory.entries, entry)
        switch (entry->type) {
            case MEMSPACE_PHY:
                kfree_phy((void*)((memory_entry_phy_t*)entry)->phy_addr);
                break;
            case MEMSPACE_STACK:
                kfree_phy((void*)((memory_entry_stack_t*)entry)->phy_addr);
                break;
            case MEMSPACE_DEVICE:
            case MEMSPACE_CACHE:
            default:
                // Can't occur in userspace
                ASSERT(false);
                break;
        }
    END_FOR_LLIST()

    memspace_deallocate(&task->memory);

    task->ret_val = ret_val;

    if (!llist_empty(task->waiters)) {
        task_t* waiter;
        FOR_LLIST(task->waiters, waiter)
            waiter->wait_ctx.wait.complete = true;
        END_FOR_LLIST()
    }

    task->run_state = TASK_COMPLETE;
}

void task_final_cleanup(task_t* task) {
    // Must be called from another task

    // Not sure how to confirm this
    //kfree_phy(task->kernel_stack_base);

    task->tid = 0;
}

uint64_t task_await(task_t* task, task_t* target_task) {
    task_clear_waiter(task, target_task);

    uint64_t ret_val = target_task->ret_val;

    if (llist_empty(target_task->waiters)) {
        task_final_cleanup(target_task);
    }

    return ret_val;
}

void task_clear_waiter(task_t* task, task_t* target_task) {
    llist_delete_ptr(target_task->waiters, task);
}

void task_add_waiter(task_t* task, task_t* target_task) {
    llist_append_ptr(target_task->waiters, task);
}

void wait_timer_setup(uint64_t now_us, uint64_t max_sleep_time) {


    uint64_t timer_fire_time = now_us + max_sleep_time;
    task_t* timer_task = NULL;

    for (uint64_t idx = 0; idx < MAX_NUM_TASKS; idx++) {
        task_t* task = &s_task_list[idx];
        if (task->run_state == TASK_WAIT) {

            uint64_t wake_time_us;

            switch (task->wait_reason) {
            case WAIT_TIMER:
                wake_time_us = task->wait_ctx.timer.wake_time_us;
                break;
            case WAIT_SELECT:
                if (!task->wait_ctx.select.timeout_valid) {
                    continue;
                }
                wake_time_us = task->wait_ctx.select.timeout_end_us;
                break;
            default:
                continue;
            }

            if (wake_time_us <= now_us) {
                continue;
            }

            if (timer_fire_time == 0) {
                timer_fire_time = wake_time_us;
                timer_task = task;
            } else if (timer_fire_time > wake_time_us) {
                timer_fire_time = wake_time_us;
                timer_task = task;
            }
        }
    }


    uint64_t downcount;
    READ_SYS_REG(CNTP_TVAL_EL0, downcount);
    if (downcount > 0) {
        //uint64_t ticks_per_us = gtimer_get_frequency() / 1000000;
        //console_log(LOG_DEBUG, "Cancel timer with %d us left", downcount / ticks_per_us);
    }

    if (now_us >= timer_fire_time) {
        //console_log(LOG_DEBUG, "Timer expired in the past");
        timer_fire_time = now_us + 1;
    }

    (void)timer_task;
    /*
    console_log(LOG_DEBUG, "Timer set for %s in %d us (%d %d)",
                timer_task != NULL ? timer_task->name : "Preemept",
                timer_fire_time - now_us,
                timer_fire_time, now_us);
                */
    gtimer_start_downtimer_us(timer_fire_time - now_us, true);
}

void schedule(void) {

    ASSERT(s_active_task_idx < MAX_NUM_TASKS ||
           s_active_task_idx == UINT64_MAX);
    uint64_t task_count;
    uint64_t task_idx;

    if (s_active_task_idx < MAX_NUM_TASKS) {
        elapsedtimer_stop(&s_task_list[s_active_task_idx].profile_time);
    }


    //while (gic_try_irq_handler());

    elapsedtimer_start(&s_idletime);

    uint64_t now_us = gtimer_get_count_us();

    // Round Robin
    for (task_count = 0; task_count < MAX_NUM_TASKS; task_count++) {
        task_idx = (task_count + s_active_task_idx + 1) % MAX_NUM_TASKS;
        if (s_task_list[task_idx].tid != 0) {

            if (s_task_list[task_idx].run_state == TASK_RUNABLE ||
                s_task_list[task_idx].run_state == TASK_AWAKE) {
                    break;
            } else if (s_task_list[task_idx].run_state == TASK_WAIT &&
                    s_task_list[task_idx].wait_canwakeup_fn != NULL) {

                if (s_task_list[task_idx].wait_canwakeup_fn(&s_task_list[task_idx].wait_ctx)) {
                    s_task_list[task_idx].run_state = TASK_AWAKE;
                    break;
                }
            }
        }
    }

    wait_timer_setup(now_us, TASK_MAX_PROC_TIME_US);
    (void)now_us;


    if (task_count < MAX_NUM_TASKS) {
        s_active_task_idx = task_idx;

        // console_log(LOG_DEBUG, "Scheduling %d (%s)", s_task_list[s_active_task_idx].tid, s_task_list[s_active_task_idx].name);

        elapsedtimer_stop(&s_idletime);
        elapsedtimer_start(&s_task_list[s_active_task_idx].profile_time);

        int64_t reg_x0 = 0;
        switch (s_task_list[task_idx].run_state) {
            case TASK_RUNABLE:
                restore_context(s_task_list[task_idx].tid);
                break;
            case TASK_AWAKE:
                if (s_task_list[task_idx].wait_wakeup_fn != NULL) {
                    reg_x0 = s_task_list[task_idx].wait_wakeup_fn(&s_task_list[task_idx]);
                }
                s_task_list[task_idx].run_state = TASK_RUNABLE;
                restore_context_kernel(s_task_list[task_idx].tid, reg_x0, s_task_list[task_idx].kernel_wait_sp);
                break;
            default:
                ASSERT(0);
                break;
        }
    } else {
        // Schedule idle task
        s_active_task_idx = UINT64_MAX;
        restore_context_idle();
    }

    ASSERT(0);
}

int64_t find_open_fd(task_t* task) {

    for (int idx = 0; idx < MAX_TASK_FDS; idx++) {
        if (!task->fds[idx].valid) {
            return idx;
        }
    }

    return -1;
}

bool signal_canwakeup_fn(wait_ctx_t* wait_ctx) {
    return *wait_ctx->signal.trywake;
}

bool timer_canwakeup_fn(wait_ctx_t* wait_ctx) {
    //console_log(LOG_DEBUG, "Check timer %d >= %d", gtimer_get_count_us(), wait_ctx->timer.wake_time_us);
    return gtimer_get_count_us() >= wait_ctx->timer.wake_time_us;
}

void task_wait_timer_at(uint64_t wake_time_us) {

    task_t* task = get_active_task();

    wait_ctx_t timer_ctx = {
        .timer = {
            .wake_time_us = wake_time_us
        }
    };

    task_wait_kernel(task,
                     WAIT_TIMER,
                     &timer_ctx,
                     NULL,
                     timer_canwakeup_fn);
}

void task_wait_timer_in(uint64_t delay_us) {
    task_wait_timer_at(gtimer_get_count_us() + delay_us);
}