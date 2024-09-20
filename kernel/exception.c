
#include <stdint.h>
#include <stdbool.h>

#include "stdlib/bitutils.h"
#include "kernel/exception.h"
#include "kernel/panic.h"
#include "kernel/interrupt/interrupt.h"
#include "kernel/task.h"
#include "kernel/schedule.h"
#include "kernel/console.h"
#include "kernel/kernelspace.h"
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
    console_printf("Fault Addr %x", elr);

    if (exception_num == 0) {
        if (IS_KSPACE_PTR(elr)) {
            console_printf(" (Instr: %8x)", *(uint32_t*)elr);
        } else {
            task_t* t = get_active_task();
            uint64_t fault_addr_phy;
            bool ok = vmem_walk_table(t->low_vm_table, elr, &fault_addr_phy);
            if (ok) {
                uint32_t* fault_addr = PHY_TO_KSPACE_PTR(fault_addr_phy);
                console_printf(" (Instr kptr: %8x %16x)", *fault_addr, fault_addr);
                console_printf(" (Instr elr: %8x)", *(uint32_t*)elr);
                uint64_t check_addr = vmem_check_address(elr);
                console_printf(" (S12E1R PAR: %16x)", check_addr);
                check_addr = vmem_check_address_user(elr);
                console_printf(" (S12E0R PAR: %16x)", check_addr);
            } else {
                console_printf(" (Can't translate addr)");
            }
        }
    }
    console_printf("\n");

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
        task_t* t = get_active_task();
        if (IS_USER_TASK(t->tid)) {
            console_log(LOG_INFO, "Unhandled Exception in user task %s", t->name);
            task_cleanup(t, -1);
            schedule();
        } else {
            PANIC("Unhandled Exception");
        }
    } else {
        handler(vector, esr);
    }
}

void exception_handler_sync_lower(uint64_t vector) {
    panic("Exception", vector, "Sync Lower");
}

uint64_t exception_handler_irq(uint64_t vector, irq_stackframe_t* frame) {

    interrupt_handle_irq_entry(vector);

    if (get_active_task() == NULL) {
        schedule_from_irq();
    }

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
        PANIC("Unhandled Kernel Exception");
    } else {
        handler(vector, esr, frame);
    }

    return 0;
}
