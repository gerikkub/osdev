
#include <stdint.h>
#include <stdbool.h>

#include "stdlib/bitutils.h"
#include "kernel/exception.h"
#include "kernel/panic.h"
#include "kernel/interrupt/interrupt.h"
#include "kernel/task.h"
#include "stdlib/printf.h"

void unhandled_exception(uint64_t exception_num) {
    uint32_t esr;
    uint32_t far;

    READ_SYS_REG(ESR_EL1, esr);
    READ_SYS_REG(FAR_EL1, far);

    console_printf("Unhandled exception: %d\n", exception_num);
    console_printf("ESR: %8x\n", esr);
    console_printf("FAR: %x\n", far);

    uint64_t elr;
    READ_SYS_REG(ELR_EL1, elr);
    console_printf("Fault Addr %x\n", elr);

    task_t* active_task = get_active_task();

    console_printf("tid %u\n", active_task->tid);
    console_printf("name %s\n", active_task->name);
}

void exception_handler_sync(uint64_t vector) {

    uint32_t esr;

    READ_SYS_REG(ESR_EL1, esr);

    uint32_t ec = (esr >> 26) & 0x3F;

    sync_handler handler = get_sync_handler(ec);

    if (handler == 0) {
        unhandled_exception(ec);
        PANIC("Unhandled Exception");
    } else {
        handler(vector, esr);
    }
}

void exception_handler_sync_lower(uint64_t vector) {
    panic("Exception", vector, "Sync Lower");
}

void gic_irq_handler(uint32_t vector);

uint64_t exception_handler_irq(uint64_t vector, irq_stackframe_t* frame) {
    gic_irq_handler(vector);

    schedule();

    return 0;
}

void exception_handler_irq_lower(uint64_t vector) {
    panic("Exception", vector, "IRQ Lower");
}

void panic_exception_handler(uint64_t vector) {
    panic("Exception", vector, "Exception in SP1");
}

uint64_t exception_handler_sync_kernel(uint64_t vector, irq_stackframe_t* frame) {

    uint32_t esr;

    READ_SYS_REG(ESR_EL1, esr);

    uint32_t ec = (esr >> 26) & 0x3F;

    kernel_sync_handler handler = get_kernel_sync_handler(ec);

    if (handler == 0) {
        unhandled_exception(ec);
        PANIC("Unhandled Exception");
    } else {
        handler(vector, esr, frame);
    }

    return 0;
}