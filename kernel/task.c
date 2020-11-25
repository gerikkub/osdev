

#include <stdint.h>
#include <stdbool.h>

#include "kernel/task.h"
#include "kernel/assert.h"
#include "kernel/kmalloc.h"
#include "kernel/vmem.h"
#include "kernel/exception.h"
#include "kernel/memoryspace.h"
#include "kernel/kernelspace.h"

static task_t s_task_list[MAX_NUM_TASKS] = {0};

static uint64_t s_active_task_idx;

static uint64_t s_max_tid;

static _vmem_table* s_dummy_user_table;

static uint64_t* s_exstack;

void switch_to_kernel_stack_asm(uint64_t vector,
                                uint64_t cpu_sp,
                                uint64_t task_sp,
                                exception_handler func);

void task_init(uint64_t* exstack) {
    ASSERT(exstack != NULL);

    s_max_tid = 1;
    s_dummy_user_table = vmem_allocate_empty_table();
    s_exstack = exstack;
}

task_t* get_active_task(void) {
    ASSERT(s_active_task_idx < MAX_NUM_TASKS);

    return &s_task_list[s_active_task_idx];
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

    ASSERT(idx < MAX_NUM_TASKS);

    return &s_task_list[idx];
}

void save_context(task_reg_t* state_fp,
                  uint64_t vector,
                  exception_handler func,
                  uint64_t* cpu_kernel_stack) {

    ASSERT(state_fp != NULL);
    ASSERT(func != NULL);

    uint64_t task_tid;
    READ_SYS_REG(TPIDR_EL0, task_tid);

    task_t* task = &s_task_list[s_active_task_idx];
    ASSERT(task_tid == task->tid);

    task->reg = *state_fp;

    task->run_state = TASK_RUNABLE;

    if (VECTOR_FROM_LOW_64(vector) &&
        (VECTOR_IS_SYNC(vector) ||
         VECTOR_IS_SERROR(vector))) {
        
        switch_to_kernel_stack_asm(vector, (uint64_t)cpu_kernel_stack, (uint64_t)task->kernel_stack_base, func);
    } else {
        func(vector);
    }

    ASSERT(1); // Should not reach
}

void restore_context(uint64_t tid) {

    task_t* task = get_task_for_tid(tid);

    WRITE_SYS_REG(TPIDR_EL0, tid);

    uint64_t sp = task->reg.sp;
    WRITE_SYS_REG(SP_EL0, sp);

    vmem_set_user_table((_vmem_table*)KSPACE_TO_PHY(task->low_vm_table), tid);

    restore_context_asm(&task->reg, s_exstack);

    ASSERT(1); // Should not reach
}

uint64_t create_task(uint64_t* user_stack_base,
                     uint64_t user_stack_size,
                     uint64_t* kernel_stack_base,
                     uint64_t kernel_stack_size,
                     task_reg_t* reg,
                     memory_space_t* memspace,
                     bool kernel_task) {

    ASSERT(kernel_stack_base != NULL);
    ASSERT(kernel_stack_size > 0);
    ASSERT(user_stack_base != NULL || kernel_task);
    ASSERT(reg->elr != NULL);

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

    task->user_stack_base = user_stack_base;
    task->user_stack_size = user_stack_size;

    task->kernel_stack_base = kernel_stack_base;
    task->kernel_stack_size = kernel_stack_size;

    task->reg = *reg;

    msg_queue_init(&task->msgs);

    if (memspace != NULL) {
        task->memory = *memspace;
        task->low_vm_table = memspace_build_vmem(memspace);
    } else {
        memory_space_t null_memspace = {
            .entries = NULL,
            .num = 0,
            .maxnum = 0
        };
        task->memory = null_memspace;
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
    } else {
        task->reg.sp = (uint64_t)&kernel_stack_base[-2];
        task->reg.gp[TASK_REG_FP] = (uint64_t)&kernel_stack_base[-2];
    }
    kernel_stack_base[-1] = (uint64_t)bad_task_return;
    kernel_stack_base[-2] = (uint64_t)&kernel_stack_base[-2];

    return task->tid;
}

uint64_t create_kernel_task(uint64_t stack_size,
                            task_f task_entry,
                             void* ctx) {

    uint64_t* stack_ptr = (uint64_t*)kmalloc_phy(stack_size);
    ASSERT(stack_ptr != NULL);

    task_reg_t reg;
    reg.gp[TASK_REG(0)] = (uint64_t)ctx;
    reg.spsr = TASK_SPSR_M(4); // EL1t SP
    reg.elr = (uint64_t)task_entry;

    return create_task(NULL, 0, stack_ptr, stack_size, &reg, NULL, true);
}

uint64_t create_system_task(uint64_t kernel_stack_size,
                            uintptr_t user_stack_base,
                            uint64_t user_stack_size,
                            memory_space_t* memspace,
                            task_f task_entry,
                            void* ctx) {

    uint64_t* kernel_stack_ptr = (uint64_t*)kmalloc_phy(kernel_stack_size);
    ASSERT(kernel_stack_ptr != NULL);

    memory_entry_stack_t  kernel_stack_entry = {
        .start = (uintptr_t)kernel_stack_ptr,
        .end = PAGE_CEIL(((uintptr_t)kernel_stack_ptr) + kernel_stack_size),
        .type = MEMSPACE_PHY,
        .flags = MEMSPACE_FLAG_PERM_URW,
        .phy_addr = KSPACE_TO_PHY((uintptr_t)kernel_stack_ptr & 0xFFFFFFFFFFFF),
        .base = (uintptr_t)kernel_stack_ptr,
        .limit = PAGE_CEIL(((uintptr_t)kernel_stack_ptr) + kernel_stack_size),
        .maxlimit = PAGE_CEIL(((uintptr_t)kernel_stack_ptr) + kernel_stack_size)
    };

    bool memspace_result;
    memspace_result = memspace_add_entry_to_kernel_memory((memory_entry_t*)&kernel_stack_entry);
    if (!memspace_result) {
        kfree_phy((uint8_t*)kernel_stack_ptr);
        return 0;
    }

    task_reg_t reg;
    for (uint64_t idx = 0; idx < 31; idx++) {
        reg.gp[TASK_REG(idx)] = 0;
    }
    reg.gp[TASK_REG(0)] = (uint64_t)ctx;
    reg.spsr = TASK_SPSR_M(0); // EL0t SP
    reg.elr = (uint64_t)task_entry;

    return create_task((uint64_t*)user_stack_base, user_stack_size,
                       (uint64_t*)kernel_stack_entry.base, kernel_stack_size,
                       &reg, memspace, false);
}

uint64_t create_user_task(uint64_t user_stack_size,
                            uint64_t kernel_stack_size,
                            memory_space_t memspace,
                            task_f task_entry,
                            void* ctx){
    return 0;
}

void task_wait(task_t* task, wait_reason_t reason, wait_ctx_t ctx, task_wakeup_f wakeup_fun) {
    ASSERT(task != NULL);

    task->run_state = TASK_WAIT;
    task->wait_reason = reason;
    task->wait_ctx = ctx;
    task->wait_wakeup_fun = wakeup_fun;

    schedule();
}

void task_wakeup(task_t* task, wait_reason_t reason, task_canwakeup_f can_wake_fun) {
    ASSERT(task != NULL);

    if ((task->run_state == TASK_RUNABLE) ||
        (task->wait_reason != reason)) {
        return;
    }

    if (can_wake_fun(&task->wait_ctx)) {
        task->run_state = TASK_AWAKE;
    }
}

void schedule(void) {

    ASSERT(s_active_task_idx < MAX_NUM_TASKS);
    uint64_t task_count;
    uint64_t task_idx;

    // Round Robin
    for (task_count = 0; task_count < MAX_NUM_TASKS; task_count++) {
        task_idx = (task_count + s_active_task_idx + 1) % MAX_NUM_TASKS;
        if (s_task_list[task_idx].tid != 0 &&
            ((s_task_list[task_idx].run_state == TASK_RUNABLE) ||
             (s_task_list[task_idx].run_state == TASK_AWAKE)))  {
                break;
        }
    }

    ASSERT(task_count < MAX_NUM_TASKS);

    s_active_task_idx = task_idx;

    int64_t reg_x0;
    switch (s_task_list[task_idx].run_state) {
        case TASK_RUNABLE:
            restore_context(s_task_list[task_idx].tid);
            break;
        case TASK_AWAKE:
            reg_x0 = s_task_list[task_idx].wait_wakeup_fun(&s_task_list[task_idx]);
            s_task_list[task_idx].reg.gp[TASK_REG(0)] = reg_x0;
            s_task_list[task_idx].run_state = TASK_RUNABLE;
            restore_context(s_task_list[task_idx].tid);
            break;
        default:
            ASSERT(0);
            break;
    }

    ASSERT(0);
}
