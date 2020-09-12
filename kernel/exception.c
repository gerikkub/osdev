
#include <stdint.h>

#include "kernel/panic.h"

void unhandled_exception(uint64_t exception_num) {
    panic("Exception", exception_num, "Unhandled exception");
}

void exception_handler_sync(void) {
    panic("Exception", 0, "Sync");
}

void exception_handler_sync_lower(void) {
    panic("Exception", 0, "Sync Lower");
}

void exception_handler_irq(void) {
    panic("Exception", 0, "IRQ");
}

void exception_handler_irq_lower(void) {
    panic("Exception", 0, "IRQ Lower");
}
