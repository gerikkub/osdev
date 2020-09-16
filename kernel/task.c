

#include <stdint.h>
#include <stdbool.h>

#include "kernel/task.h"
#include "kernel/assert.h"
#include "kernel/kmalloc.h"
#include "kernel/vmem.h"
#include "kernel/exception.h"

static task_t s_task_list[MAX_NUM_TASKS] = {0};

static uint64_t s_max_tid;

static uint8_t s_stack[4096];

void task_init(void) {
    s_max_tid = 1;
}

static task_t* get_task_for_tid(uint64_t tid) {

    uint64_t idx;
    for (idx = 0; idx < MAX_NUM_TASKS; idx++) {
        if (tid == s_task_list[idx].tid) {
            break;
        }
    }

    ASSERT(idx < MAX_NUM_TASKS);

    return &s_task_list[idx];
}

void save_context(task_reg_t* task_reg,
                  uint64_t task_sp,
                  uint64_t task_lr,
                  uint64_t task_spsr,
                  uint64_t vector,
                  exception_handler func) {

    ASSERT(task_reg != NULL);
    ASSERT(func != NULL);

    uint64_t task_tid;
    READ_SYS_REG(TPIDR_EL0, task_tid);

    task_t* task;

    task = get_task_for_tid(task_tid);

    // TODO: Get memcpy
    uint64_t idx;
    for (idx = 0; idx < 31; idx++) {
        task->reg.gp[idx] = task_reg->gp[idx];
    }
    task->sp = task_sp;
    task->lr = task_lr;
    task->spsr = task_spsr;

    func(vector);

    ASSERT(1); // Should not reach
}

void restore_context(uint64_t tid) {

    task_t* task = get_task_for_tid(tid);

    WRITE_SYS_REG(TPIDR_EL0, tid);

    uint64_t sp = task->sp;
    WRITE_SYS_REG(SP_EL0, sp);

    restore_context_asm(&task->reg,
                        task->lr,
                        task->spsr);

    ASSERT(1); // Should not reach
}

uint64_t create_task(uint64_t stack_size,
                     task_f task_entry,
                     void* ctx,
                     _vmem_table* vm_table,
                     bool kernel_task) {

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

    //task->stack_base = kmalloc_phy(stack_size);
    //task->stack_size = stack_size;
    task->stack_base = s_stack;
    task->stack_size = 4096;

    task->reg.gp[0] = (uint64_t)ctx;
    task->sp = (uint64_t)task->stack_base;
    task->lr = (uint64_t)task_entry;
    task->spsr = 0;

    if (kernel_task) {
        task->spsr |= TASK_SPSR_M(4); // EL1t SP
    }

    task->low_vm_table = vm_table;

    return task->tid;
}


