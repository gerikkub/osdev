
#include <stdint.h>

#include "kernel/exception.h"
#include "kernel/pagefault.h"
#include "kernel/assert.h"
#include "kernel/panic.h"

void pagefault_handler(uint32_t vector, uint32_t esr) {

    uint32_t ec = esr >> 26;

    switch (ec) {
        case EC_INST_ABORT_LOWER:
        case EC_INST_ABORT:
            PANIC("Pagefault on instruction");
            break;
        case EC_DATA_ABORT_LOWER:
        case EC_DATA_ABORT:
            PANIC("Pagefault on data");
            break;
        default:
            PANIC("Unknown Pagefault");
            break;
    }
}


void pagefault_init(void) {

    set_sync_handler(EC_INST_ABORT_LOWER, &pagefault_handler);
    set_sync_handler(EC_INST_ABORT, &pagefault_handler);
    set_sync_handler(EC_DATA_ABORT_LOWER, &pagefault_handler);
    set_sync_handler(EC_DATA_ABORT, &pagefault_handler);

}


