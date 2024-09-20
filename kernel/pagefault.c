
#include <stdint.h>

#include "kernel/exception.h"
#include "kernel/pagefault.h"
#include "kernel/panic.h"
#include "kernel/task.h"
#include "kernel/kernelspace.h"
#include "kernel/lib/vmalloc.h"
#include "kernel/interrupt/interrupt.h"

#include "stdlib/printf.h"

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

void pagefault_print_kernel_backtrace(task_t* task, irq_stackframe_t* frame) {

    struct stackframe_t {
        uint64_t fp;
        uint64_t lr;
    };

    console_printf("Backtrace:\n");

    console_printf("%16x ", frame->elr);

    uint64_t frame_addr = frame->gp[29];

    uint64_t depth = 0;
    while (depth < 10) {
        uint64_t phy_addr;
        bool walk_ok = false;
        if (IS_KSPACE_PTR(frame_addr)) {
            walk_ok = kspace_vmem_walk_table(frame_addr, &phy_addr);
        } else if (task->low_vm_table != NULL) {
            walk_ok = vmem_walk_table(task->low_vm_table, frame_addr, &phy_addr);
        }
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

void pagefault_kernel_handler(uint64_t vector, uint32_t esr, irq_stackframe_t* frame) {

    static int in_pagefault = 0;

    if (in_pagefault == 0) {
        in_pagefault = 1;

        uint32_t ec = esr >> 26;

        console_printf("Kernel Pagefault in vector %u\n", vector);

        console_printf("ESR %x\n", esr);

        if ((esr & (1 << 10)) == 0) {
            uint64_t far;
            READ_SYS_REG(FAR_EL1, far);
            console_printf("FAR %x\n", far);
        } else {
            console_printf("FAR Invalid\n");
        }

        console_printf("Fault Addr %x\n", frame->elr);
        console_printf("Fault SPSR %x\n", frame->spsr);

        task_t* active_task = get_active_task();

        console_printf("tid %u\n", active_task->tid);
        console_printf("name %s\n", active_task->name);

        pagefault_print_kernel_backtrace(active_task, frame);

        console_printf("Registers:\n");
        for (uint32_t idx = 0; idx < 31; idx++) {
            if (idx < 10) {
                console_printf("X%d:  %16x ",
                               idx, frame->gp[idx]);
            } else {
                console_printf("X%d: %16x ",
                               idx, frame->gp[idx]);
            }

            if (idx%2 == 1) {
                console_printf("\n");
            }
        }

        console_printf("\n");

        switch (ec) {
            case EC_INST_ABORT:
                PANIC("Kernelspace Pagefault on instruction");
                break;
            case EC_DATA_ABORT:
                PANIC("Kernelspace Pagefault on data");
                break;
            default:
                PANIC("Unknown Pagefault");
                break;
        }

    } else if (in_pagefault == 1) {
        in_pagefault = 2;
        PANIC("Recursive Pagefault");
    } else {
        DISABLE_IRQ();
        while (1) {}
    }

}

bool pagefault_handler_kern(uint32_t esr) {

    uint8_t far_notvalid = (esr >> 10) & 1;
    uint8_t cm = (esr >> 8) & 1;
    uint8_t wnr = (esr >> 6) & 1;
    uint8_t dfsc = esr & 0x3F;

    if (!(!far_notvalid &&
          !cm &&
          !wnr &&
          dfsc >= 4 && dfsc < 8)) {

        return false;
    }

    // Check if a valid cache mapping exists for the memory space in question
    uint64_t far;
    READ_SYS_REG(FAR_EL1, far);

    if (!((far >> 63) & 1)) {
        // Only handle kernel addresses
        return false;
    }

    memory_entry_t* mem_entry = memspace_get_entry_at_addr_kernel((void*)far);
    if (mem_entry == NULL) {
        return false;
    }

    if (mem_entry->type != MEMSPACE_CACHE) {
        return false;
    }

    // Handle a read from a cache entry
    memory_entry_cache_t* memcache_entry = (memory_entry_cache_t*)mem_entry;
    memcache_phy_entry_t* new_phy_entry = vmalloc(sizeof(memcache_phy_entry_t));
    bool ok;
    ok = memcache_entry->cacheops_ptr->populate_virt_fn(memcache_entry->cacheops_ctx, far, new_phy_entry);

    if (!ok) {
        return false;
    }

    llist_append_ptr(memcache_entry->phy_addr_list, new_phy_entry);

    memspace_update_kernel_vmem();

    return true;
}

void pagefault_init(void) {

    set_sync_handler(EC_INST_ABORT_LOWER, &pagefault_handler);
    set_sync_handler(EC_INST_ABORT, &pagefault_handler);
    set_sync_handler(EC_DATA_ABORT_LOWER, &pagefault_handler);
    set_sync_handler(EC_DATA_ABORT, &pagefault_handler);

    set_kernel_sync_handler(EC_INST_ABORT_LOWER, &pagefault_kernel_handler);
    set_kernel_sync_handler(EC_INST_ABORT, &pagefault_kernel_handler);
    set_kernel_sync_handler(EC_DATA_ABORT_LOWER, &pagefault_kernel_handler);
    set_kernel_sync_handler(EC_DATA_ABORT, &pagefault_kernel_handler);
}


