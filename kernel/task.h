
#ifndef __TASK_H__
#define __TASK_H__

#include <stdint.h>
#include <stdbool.h>

#include "kernel/vmem.h"
#include "kernel/exception.h"
#include "stdlib/bitutils.h"
#include "kernel/memoryspace.h"
#include "kernel/messages.h"
#include "kernel/fd.h"
#include "kernel/lock/lock.h"
#include "kernel/lib/elapsedtimer.h"

#include "k_syscall.h"

#define MAX_NUM_TASKS 64

#define TASK_STD_STACK_SIZE 8192
#define KERNEL_STD_STACK_SIZE 8192

#define MAX_TASK_NAME_LEN 16

#define MAX_TASK_FDS 64

#define GET_CURR_TID(x) \
    do { \
        uint64_t _x = 0; \
        READ_SYS_REG(TPIDR_EL0, _x); \
        x = _x & 0x7FFFFFFF; \
    } while(0)

#define TASK_TID_KERNEL BIT(31)

#define IS_KERNEL_TASK(tid) ((tid) & TASK_TID_KERNEL)
#define IS_USER_TASK(tid) (!((tid) & TASK_TID_KERNEL))

#define TASK_MAX_PROC_TIME_US 5000
#define TASK_MAX_WFI_TIME_US 1000000

struct task_t_;

typedef void (*task_f)(void* ctx);

typedef struct __attribute__((packed)) {
    uint64_t spsr;
    uint64_t sp;
    uint64_t elr;
    uint64_t gp[31];
} task_reg_t;

typedef enum {
    TASK_RUNABLE,
    TASK_RUNABLE_KERNEL,
    TASK_WAIT,
    TASK_AWAKE,
} run_state_t;

typedef enum {
    WAIT_LOCK = 1,
    WAIT_GETMSGS = 2,
    WAIT_IRQNOTIFY = 3,
    WAIT_VIRTIOIRQ = 4,
    WAIT_SIGNAL = 5,
    WAIT_TIMER = 6,
    WAIT_SELECT = 7
} wait_reason_t;

typedef struct {
    lock_t* lock_ptr;
} wait_lock_t;

typedef struct {
    msg_queue* wait_queue;
} wait_getmsgs_t;

typedef struct {
    uint64_t irq;
} wait_irqnotify_t;

typedef struct {
    uint64_t x0;
} wait_initthread_t;

typedef struct {
    void* ctx;
} wait_virtioirq_t;

typedef struct {
    bool* trywake;
} wait_signal_t;

typedef struct {
    uint64_t wake_time_us;
} wait_timer_t;

typedef struct {
    struct task_t_* task;
    syscall_select_ctx_t* select_arr;
    uint64_t select_len;
    bool timeout_valid;
    int64_t timeout_end_us;
    uint64_t* ready_mask_out;
} wait_select_t;

typedef union {
    wait_lock_t lock;
    wait_getmsgs_t getmsgs;
    wait_irqnotify_t irqnotify;
    wait_initthread_t init_thread;
    wait_virtioirq_t virtioirq;
    wait_signal_t signal;
    wait_timer_t timer;
    wait_select_t select;
} wait_ctx_t;

typedef bool (*task_canwakeup_f)(wait_ctx_t* wait_ctx);
typedef int64_t (*task_wakeup_f)(struct task_t_*);


typedef struct task_t_ {

    uint32_t tid;
    uint8_t asid;
    char name[MAX_TASK_NAME_LEN];

    run_state_t run_state;
    wait_reason_t wait_reason;
    wait_ctx_t wait_ctx;
    task_wakeup_f wait_wakeup_fn;
    task_canwakeup_f wait_canwakeup_fn;

    fd_ctx_t fds[MAX_TASK_FDS];

    uint64_t* user_stack_base;
    uint64_t user_stack_size;

    uint64_t* kernel_stack_base;
    uint64_t kernel_stack_size;

    task_reg_t reg;
    void* kernel_wait_sp;

    memory_space_t memory;
    _vmem_table* low_vm_table;

    msg_queue msgs;

    elapsedtimer_t profile_time;
} task_t;

void task_init(uint64_t* exstack);
task_t* get_active_task(void);
task_t* get_task_at_idx(int64_t idx);
task_t* get_task_for_tid(uint64_t tid);
uint64_t get_schedule_profile_time(void);
void restore_context(uint64_t tid);
void restore_context_kernel(uint64_t tid, uint64_t x0, void* sp);
void restore_context_kernel_asm(uint64_t x0, void* sp);

uint64_t create_task(uint64_t* user_stack_base,
                     uint64_t user_stack_size,
                     uint64_t* kernel_stack_base,
                     uint64_t kernel_stack_size,
                     task_reg_t* reg,
                     memory_space_t* vm_table,
                     bool kernel_task,
                     const char* name);
uint64_t create_kernel_task(uint64_t stack_size, task_f task_entry, void* ctx, const char* name);
uint64_t create_system_task(uint64_t kernel_stack_size,
                            uintptr_t user_stack_base,
                            uint64_t user_stack_size,
                            memory_space_t* memspace,
                            task_f task_entry,
                            void* ctx,
                            const char* name);
uint64_t create_user_task(uint64_t kernel_stack_size,
                          uintptr_t user_stack_base,
                          uint64_t user_stack_size,
                          memory_space_t* memspace,
                          task_f task_entry,
                          void* ctx,
                          const char* name);
void restore_context_asm(task_reg_t* reg,
                         uint64_t task_sp,
                         uint64_t handler_sp,
                         uint64_t currentSP);

// void task_wait(task_t* task, wait_reason_t reason, wait_ctx_t ctx, task_wakeup_f wakeup_fun);
void task_wakeup(task_t* task, wait_reason_t reason);

void task_cleanup(task_t* task, uint64_t ret_val);

void schedule(void);

// Implemented in exception_asm.s
int64_t task_wait_kernel(task_t* task,
                         wait_reason_t reason,
                         wait_ctx_t* ctx,
                         task_wakeup_f wakeup_fun,
                         task_canwakeup_f canwakeup_f);

int64_t find_open_fd(task_t* task);

bool signal_canwakeup_fn(wait_ctx_t* wait_ctx);

void task_wait_timer_at(uint64_t wake_time_us);
void task_wait_timer_in(uint64_t delay_us);

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
