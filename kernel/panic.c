
#include "kernel/task.h"
#include "kernel/console.h"
#include "kernel/assert.h"
#include "kernel/interrupt/interrupt.h"

#include "stdlib/printf.h"

void panic(char* file, uint64_t line, char* msg, ...) {
    DISABLE_IRQ();

    console_log(LOG_CRIT, "Panic: %s:%u %s\n", file, line, msg);

    uint64_t esr;
    READ_SYS_REG(ESR_EL1, esr);
    console_log(LOG_CRIT, "ESR: %8x", esr);
    console_log(LOG_CRIT, "Backtrace");

    struct stackframe_t {
        uint64_t fp;
        uint64_t lr;
    };

    uint64_t frame_addr;
    asm volatile ("mov %0, x29"
                  : "=r" (frame_addr));

    uint64_t depth = 0;
    while (depth < 16 && frame_addr != 0) {
        struct stackframe_t* frame_ptr = (struct stackframe_t*)frame_addr;
        if (!((frame_addr & 0xFFFF000000000000) == 0 ||
              (frame_addr & 0xFFFF000000000000) == 0xFFFF000000000000)) {
            break;
        }
        console_printf("%16x ", frame_ptr->lr - 4);
        if (frame_ptr->fp == (uint64_t)frame_ptr) {
            break;
        }
        frame_addr = frame_ptr->fp;
        depth++;
    }
    console_printf("\n");

    //asm ("brk #0");
    while (1) {
    }
}

void task_panic(char* file, uint64_t line, char* msg, ...) {
    task_t* t = get_active_task();
    ASSERT(t != NULL);

    console_log(LOG_CRIT, "Task Panic: %s:%u %s\n", file, line, msg);
    console_log(LOG_CRIT, "Task: %d\n", t->tid);

    while (1) {
    }
}
