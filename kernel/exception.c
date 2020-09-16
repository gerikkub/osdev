
#include <stdint.h>
#include <stdbool.h>

#include "kernel/bitutils.h"
#include "kernel/exception.h"
#include "kernel/panic.h"
#include "kernel/gic.h"

void unhandled_exception(uint64_t exception_num) {
    panic("Exception", exception_num, "Unhandled exception");
}

void exception_handler_sync(vector) {

    uint64_t esr;

    READ_SYS_REG(ESR_EL1, esr);

    uint64_t ec = (esr >> 26) & 0x3F;

    sync_handler handler = get_sync_handler(ec);

    if (handler == 0) {
        panic("Exception", vector, "Sync");
    } else {
        handler(vector, esr);
    }
}

void exception_handler_sync_lower(uint64_t vector) {
    panic("Exception", vector, "Sync Lower");
}

void exception_handler_irq(uint64_t vector) {
    gic_irq_handler(vector);
}

void exception_handler_irq_lower(uint64_t vector) {
    panic("Exception", vector, "IRQ Lower");
}
