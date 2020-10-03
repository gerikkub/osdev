
#ifndef __TASK_H__
#define __TASK_H__

#include <stdint.h>
#include <stdbool.h>

#include "kernel/vmem.h"
#include "kernel/exception.h"
#include "kernel/bitutils.h"
#include "kernel/memoryspace.h"

#define MAX_NUM_TASKS 1024

typedef void (*task_f)(void* ctx);

typedef struct __attribute__((packed)) {
    uint64_t spsr;
    uint64_t sp;
    uint64_t elr;
    uint64_t gp[31];
} task_reg_t;

typedef enum {
    TASK_RUNABLE,
    TASK_WAIT
} run_state_t;


typedef struct {

    uint32_t tid;
    uint8_t asid;
    run_state_t run_state;

    uint64_t* user_stack_base;
    uint64_t user_stack_size;

    uint64_t* kernel_stack_base;
    uint64_t kernel_stack_size;

    task_reg_t reg;

    memory_space_t memory;
    _vmem_table* low_vm_table;

} task_t;

void task_init(void);
task_t* get_active_task(void);
void restore_context(uint64_t tid);
uint64_t create_task(uint64_t* user_stack_base,
                     uint64_t user_stack_size,
                     uint64_t* kernel_stack_base,
                     uint64_t kernel_stack_size,
                     task_reg_t* reg,
                     _vmem_table* vm_table,
                     bool kernel_task);
uint64_t create_kernel_task(uint64_t stack_size, task_f task_entry, void* ctx);
uint64_t create_user_task(uint64_t user_stack_size, uint64_t kernel_stack_size, task_f task_entry, void* ctx);
void restore_context_asm(task_reg_t* reg);

void schedule(void);

#define TASK_TID_KERNEL BIT(31)

#define TASK_SPSR_N BIT(31)
#define TASK_SPSR_Z BIT(30)
#define TASK_SPSR_C BIT(29)
#define TASK_SPSR_V BIT(28)
#define TASK_SPSR_SS BIT(21)
#define TASK_SPSR_IL BIT(20)
#define TASK_SPSR_D BIT(9)
#define TASK_SPSR_A BIT(8)
#define TASK_SPSR_I BIT(7)
#define TASK_SPSR_F BIT(6)
#define TASK_SPSR_M_4 BIT(4)
#define TASK_SPSR_M_3 BIT(3)
#define TASK_SPSR_M_2 BIT(2)
#define TASK_SPSR_M_1 BIT(1)
#define TASK_SPSR_M_0 BIT(0)
#define TASK_SPSR_M(x) (x & 0xF)

#define TASK_REG(x) (x)
#define TASK_REG_FP 29
#define TASK_REG_LR 30

#endif
