
#include <stdint.h>

#include "kernel/exception.h"
#include "kernel/pagefault.h"
#include "kernel/assert.h"
#include "kernel/panic.h"
#include "kernel/task.h"
#include "kernel/console.h"
#include "kernel/kernelspace.h"

void pagefault_print_backtrace(task_t* task) {

    struct stackframe_t {
        uint64_t fp;
        uint64_t lr;
    };

    console_printf("Backtrace:\n");

    console_printf("%16x ", task->reg.elr);

    uint64_t frame_addr = task->reg.gp[29];

    uint64_t depth = 0;
    while (depth < 10) {
        uint64_t phy_addr;
        bool walk_ok;
        walk_ok = vmem_walk_table(task->low_vm_table, frame_addr, &phy_addr);
        if (walk_ok) {
            struct stackframe_t* frame_ptr = (struct stackframe_t*)PHY_TO_KSPACE(phy_addr);
            console_printf("%16x ", frame_ptr->lr - 4);
            frame_addr = frame_ptr->fp;
        } else {
            break;
        }
        depth++;
    }

    console_printf("\n");
}

void pagefault_handler(uint64_t vector, uint32_t esr) {

    uint32_t ec = esr >> 26;

    console_printf("Pagefault in vector %u\n", vector);

    console_printf("ESR %x\n", esr);

    if ((esr & (1 << 10)) == 0) {
        uint64_t far;
        READ_SYS_REG(FAR_EL1, far);
        console_printf("FAR %x\n", far);
    } else {
        console_printf("FAR Invalid\n");
    }

    uint64_t elr;
    READ_SYS_REG(ELR_EL1, elr);
    console_printf("Fault Addr %x\n", elr);

    task_t* active_task = get_active_task();

    console_printf("tid %u\n", active_task->tid);
    console_printf("name %s\n", active_task->name);

    pagefault_print_backtrace(active_task);

    console_printf("Registers:\n");
    for (uint32_t idx = 0; idx < 31; idx++) {
        if (idx < 10) {
            console_printf("X%d:  %16x ",
                           idx, active_task->reg.gp[idx]);
        } else {
            console_printf("X%d: %16x ",
                           idx, active_task->reg.gp[idx]);
        }

        if (idx%2 == 1) {
            console_printf("\n");
        }
    }

    console_printf("\n");

    switch (ec) {
        case EC_INST_ABORT_LOWER:
            PANIC("Userspace Pagefault on instruction");
            break;
        case EC_INST_ABORT:
            PANIC("Kernelspace Pagefault on instruction");
            break;
        case EC_DATA_ABORT_LOWER:
            PANIC("Userspace Pagefault on data");
            break;
        case EC_DATA_ABORT:
            PANIC("Kernelspace Pagefault on data");
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


