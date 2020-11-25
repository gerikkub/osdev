
#include <stdint.h>

#include "kernel/exception.h"
#include "kernel/pagefault.h"
#include "kernel/assert.h"
#include "kernel/panic.h"
#include "kernel/task.h"
#include "kernel/console.h"

void pagefault_handler(uint64_t vector, uint32_t esr) {

    uint32_t ec = esr >> 26;

    console_write("Pagefault in vector ");
    console_write_hex(vector);
    console_endl();

    console_write("ESR ");
    console_write_hex(esr);
    console_endl();

    if ((esr & (1 << 10)) == 0) {
        console_write("FAR ");
        uint64_t far;
        READ_SYS_REG(FAR_EL1, far);
        console_write_hex(far);
        console_endl();
    } else {
        console_write("FAR Invalid");
        console_endl();
    }

    uint64_t elr;
    console_write("Fault Addr ");
    READ_SYS_REG(ELR_EL1, elr);
    console_write_hex(elr);
    console_endl();

    task_t* active_task = get_active_task();

    console_write("tid ");
    console_write_hex(active_task->tid);
    console_endl();

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


