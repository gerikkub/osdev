
#include <stdint.h>

#include "kernel/memoryspace.h"
#include "kernel/vmem.h"
#include "kernel/assert.h"

memory_entry_t* memspace_get_entry_at_addr(memory_space_t* space, void* addr_ptr) {

    ASSERT(space != NULL);

    uintptr_t addr = (uintptr_t)addr_ptr;

    int idx;
    for (idx = 0; idx < space->num; idx++) {
        if (addr >= space->entries[idx].start &&
            addr < space->entries[idx].end) {
            
            break;
        }
    }

    if (idx < space->num) {
        return &space->entries[idx];
    } else {
        return NULL;
    }
}

bool memspace_add_entry_to_memory(memory_space_t* space, memory_entry_t* entry) {

    ASSERT(space != NULL);

    if (space->num >= space->maxnum) {
        // Not enough space to add a new entry
        return false;
    }

    int idx = space->num;
    space->entries[idx] = *entry;
    space->num++;

    return true;
}

static _vmem_ap_flags mempspace_vmem_get_vmem_flags(uint32_t flags) {

    _vmem_ap_flags vmem_flags = 0;

    switch (flags & MEMSPACE_FLAG_PERM_MASK) {
        case MEMSPACE_FLAG_PERM_URO:
            vmem_flags = VMEM_AP_U_RO;
            break;
        case MEMSPACE_FLAG_PERM_URW:
            vmem_flags = VMEM_AP_U_RW;
            break;
        case MEMSPACE_FLAG_PERM_URE:
            vmem_flags = VMEM_AP_U_RO |
                         VMEM_AP_U_E;
            break;
        case MEMSPACE_FLAG_PERM_KRO:
            vmem_flags = VMEM_AP_P_RO;
            break;
        case MEMSPACE_FLAG_PERM_KRW:
            vmem_flags = VMEM_AP_P_RW;
            break;
        case MEMSPACE_FLAG_PERM_KRE:
            vmem_flags = VMEM_AP_P_RO |
                         VMEM_AP_P_E;
            break;
        default:
            ASSERT(0);
    }

    return vmem_flags;
}

static bool memspace_vmem_add_phy(_vmem_table* table, memory_entry_phy_t* entry) {

    ASSERT(entry->start < entry->end);

    uint64_t len = entry->end - entry->start + 1;

    _vmem_ap_flags vmem_flags = mempspace_vmem_get_vmem_flags(entry->flags);

    vmem_map_address_range(table,
                           entry->phy_addr,
                           entry->start,
                           len,
                           vmem_flags,
                           VMEM_ATTR_MEM);

    return true;
}

static bool memspace_vmem_add_device(_vmem_table* table, memory_entry_device_t* entry) {

    ASSERT(entry->start < entry->end);

    uint64_t len = entry->end - entry->start + 1;

    _vmem_ap_flags vmem_flags = mempspace_vmem_get_vmem_flags(entry->flags);
    if ((vmem_flags & VMEM_AP_U_E) ||
        (vmem_flags & VMEM_AP_P_E)) {
        // Don't want to execute device memory
        return false;
    }

    vmem_map_address_range(table,
                           entry->phy_addr,
                           entry->start,
                           len,
                           vmem_flags,
                           VMEM_ATTR_DEVICE);

    return true;
}

_vmem_table* memspace_build_vmem(memory_space_t* space) {

    _vmem_table* l0_table = vmem_allocate_empty_table();
    ASSERT(l0_table != NULL);

    int idx;
    for (idx = 0; idx < space->num; idx++) {
        switch (space->entries[idx].type) {
            case MEMSPACE_PHY:
                memspace_vmem_add_phy(l0_table, (memory_entry_phy_t*)&space->entries[idx]);
                break;
            case MEMSPACE_DEVICE:
                memspace_vmem_add_device(l0_table, (memory_entry_device_t*)&space->entries[idx]);
                break;
            default:
                ASSERT(0);
        }
    }

    return l0_table;
}
