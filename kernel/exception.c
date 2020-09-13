
#include <stdint.h>
#include <stdbool.h>

#include "kernel/bitutils.h"
#include "kernel/exception.h"
#include "kernel/panic.h"
#include "kernel/gic.h"

void unhandled_exception(uint64_t exception_num) {
    panic("Exception", exception_num, "Unhandled exception");
}

void exception_handler_sync(void) {

    uint64_t esr;

    READ_SYS_REG(ESR_EL1, esr);

    uint64_t ec = (esr >> 26) & 0x3F;

    exception_handler handler = get_sync_handler(ec);

    if (handler == 0) {
        panic("Exception", 0, "Sync");
    } else {
        handler(0, esr);
    }
}

void exception_handler_sync_lower(void) {
    panic("Exception", 0, "Sync Lower");
}

void exception_handler_irq(void) {
    gic_irq_handler(0);
}

void exception_handler_irq_lower(void) {
    panic("Exception", 0, "IRQ Lower");
}
