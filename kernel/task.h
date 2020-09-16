
#ifndef __TASK_H__
#define __TASK_H__

#include <stdint.h>

#include "kernel/vmem.h"
#include "kernel/exception.h"
#include "kernel/bitutils.h"

#define MAX_NUM_TASKS 1024

typedef void (*task_f)(void* ctx);

typedef struct __attribute__((packed)) {
    uint64_t gp[31];
} task_reg_t;

typedef struct {

    uint32_t tid;
    uint8_t asid;

    uint8_t* stack_base;
    uint64_t stack_size;

    task_reg_t reg;
    uint64_t sp;
    uint64_t lr;
    uint64_t spsr;

    _vmem_table* low_vm_table;

} task_t;

void task_init(void);
void restore_context(uint64_t tid);
uint64_t create_task(uint64_t stack_size,
                     task_f task_entry,
                     void* ctx,
                     _vmem_table* vm_table,
                     bool kernel_task);

void restore_context_asm(task_reg_t* reg, uint64_t lr, uint64_t spsr);


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

#endif
