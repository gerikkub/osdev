
#include <stdint.h>

#include "kernel/memoryspace.h"
#include "kernel/vmem.h"
#include "kernel/task.h"
#include "kernel/assert.h"
#include "kernel/elf.h"
#include "kernel/lib/llist.h"

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

    uint64_t lr;
    asm("mov %[result], lr" : [result] "=r" (lr));

    entry->callsite = lr;

    int idx = space->num;
    space->entries[idx] = *entry;
    space->num++;

    return true;
}

/*
bool memspace_space_to_phy(memory_space_t* space, uintptr_t vmem_addr, uintptr_t* phy_addr) {
    ASSERT(space != NULL);
    ASSERT(space->entries != NULL);

    for (int idx = 0; idx < space->num; idx++) {
        memory_entry_t* entry = &space->entries[idx];
        if (vmem_addr >= entry->start &&
            vmem_addr < entry->end) {
            uint64_t offset = vmem_addr - entry->start;
            if (phy_addr != NULL) {
                *phy_addr = vmem_addr + offset;
            }
            return true;
        }
    }

    return false;
} */

static _vmem_ap_flags memspace_vmem_get_vmem_flags(uint32_t flags) {

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

    uint64_t len = entry->end - entry->start;

    _vmem_ap_flags vmem_flags = memspace_vmem_get_vmem_flags(entry->flags);

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

    uint64_t len = entry->end - entry->start;

    _vmem_ap_flags vmem_flags = memspace_vmem_get_vmem_flags(entry->flags);
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

static bool memspace_vmem_add_stack(_vmem_table* table, memory_entry_stack_t* entry) {

    ASSERT(entry->start < entry->end);
    ASSERT(entry->base > entry->start && entry->base <= entry->end);
    ASSERT(entry->limit >= entry->start && entry->limit < entry->end);
    ASSERT(entry->maxlimit >= entry->start && entry->maxlimit < entry->end);

    _vmem_ap_flags vmem_flags = memspace_vmem_get_vmem_flags(entry->flags);
    if ((vmem_flags & VMEM_AP_U_E) ||
        (vmem_flags & VMEM_AP_P_E)) {
        // Don't want to execute device memory
        return false;
    }

    ASSERT(entry->base > entry->limit);
    uint64_t len = entry->base - entry->limit;
    vmem_map_address_range(table,
                           entry->phy_addr,
                           entry->limit,
                           len,
                           vmem_flags,
                           VMEM_ATTR_MEM);
                        
    return true;
}

static bool memspace_vmem_add_cache(_vmem_table* table, memory_entry_cache_t* entry) {

    ASSERT(entry->start < entry->end);

    _vmem_ap_flags vmem_flags = memspace_vmem_get_vmem_flags(entry->flags);

    memcache_phy_entry_t* phy_entry;
    FOR_LLIST(entry->phy_addr_list, phy_entry)

        ASSERT(entry->start + phy_entry->offset <= entry->end);
        vmem_map_address_range(table,
                               phy_entry->phy_addr,
                               entry->start + phy_entry->offset,
                               phy_entry->len,
                               vmem_flags,
                               VMEM_ATTR_MEM);
    END_FOR_LLIST()

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
            case MEMSPACE_STACK:
                memspace_vmem_add_stack(l0_table, (memory_entry_stack_t*)&space->entries[idx]);
                break;
            case MEMSPACE_CACHE:
                memspace_vmem_add_cache(l0_table, (memory_entry_cache_t*)&space->entries[idx]);
                break;
            default:
                ASSERT(0);
        }
    }

    return l0_table;
}

bool memspace_alloc_space(memory_space_t* space, uint64_t len, memory_entry_t* entry_out) {
    ASSERT(space != NULL);
    ASSERT(entry_out != NULL);

    uint64_t guard_range = PAGE_CEIL(len) + 2 * USER_GUARD_PAGES;

    uintptr_t start_addr = space->valloc_ctx.systemspace_end + USER_GUARD_PAGES;
    uintptr_t stop_addr = start_addr + PAGE_CEIL(len);

    if ((stop_addr + USER_GUARD_PAGES) > USER_ADDRSPACE_TOP) {
        return false;
    }

    entry_out->start = start_addr;
    entry_out->end = stop_addr;

    space->valloc_ctx.systemspace_end += guard_range;

    return true;
}

void memspace_deallocate(memory_space_t* space) {

    //TODO: Implement this
    // We can always download more RAM...

}