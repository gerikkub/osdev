
#include <stdint.h>
#include <string.h>

#include "kernel/console.h"
#include "kernel/memoryspace.h"
#include "kernel/vmem.h"
#include "kernel/task.h"
#include "kernel/assert.h"
#include "kernel/elf.h"
#include "kernel/lib/llist.h"
#include "kernel/lib/vmalloc.h"

memory_entry_t* memspace_get_entry_at_addr(memory_space_t* space, void* addr_ptr) {

    ASSERT(space != NULL);

    uintptr_t addr = (uintptr_t)addr_ptr;

    memory_entry_t* entry = NULL;
    bool found = false;
    FOR_LLIST(space->entries, entry)
        if (addr >= entry->start &&
            addr < entry->end) {
            found = true;
            break;
        }
    END_FOR_LLIST()

    if (found) {
        return entry;
    } else {
        return NULL;
    }
}

bool memspace_add_entry_to_memory(memory_space_t* space, memory_entry_t* entry) {

    ASSERT(space != NULL);

    memory_entry_t* new_entry = memspace_alloc_entry();
    memcpy(new_entry, entry, sizeof(memory_entry_t));

    llist_append_ptr(space->new_entries, new_entry);

    uint64_t lr;
    asm("mov %[result], lr" : [result] "=r" (lr));

    entry->callsite = lr;

    return true;
}

void memspace_remove_entry_from_memory(memory_space_t* space, memory_entry_t* entry) {

    ASSERT(space != NULL);

    memory_entry_t* new_entry = memspace_alloc_entry();
    memcpy(new_entry, entry, sizeof(memory_entry_t));

    llist_append_ptr(space->del_entries, new_entry);
}

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

    ASSERT(len > 0);

    vmem_map_address_range(table,
                           entry->phy_addr,
                           entry->start,
                           len,
                           vmem_flags,
                           VMEM_ATTR_MEM,
                           !(entry->flags & MEMSPACE_FLAG_IGNORE_DUPS));

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

    ASSERT(len > 0);

    vmem_map_address_range(table,
                           entry->phy_addr,
                           entry->start,
                           len,
                           vmem_flags,
                           VMEM_ATTR_DEVICE,
                           !(entry->flags & MEMSPACE_FLAG_IGNORE_DUPS));

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
    ASSERT(len > 0);

    vmem_map_address_range(table,
                           entry->phy_addr,
                           entry->limit,
                           len,
                           vmem_flags,
                           VMEM_ATTR_MEM,
                           !(entry->flags & MEMSPACE_FLAG_IGNORE_DUPS));
                        
    return true;
}

static bool memspace_vmem_add_cache(_vmem_table* table, memory_entry_cache_t* entry) {

    ASSERT(entry->start < entry->end);

    _vmem_ap_flags vmem_flags = memspace_vmem_get_vmem_flags(entry->flags);

    memcache_phy_entry_t* phy_entry;
    FOR_LLIST(entry->phy_addr_list, phy_entry)

        ASSERT(entry->start + phy_entry->offset <= entry->end);
        ASSERT(phy_entry->len > 0);
        vmem_map_address_range(table,
                               phy_entry->phy_addr,
                               entry->start + phy_entry->offset,
                               phy_entry->len,
                               vmem_flags,
                               VMEM_ATTR_MEM,
                               false);
    END_FOR_LLIST()

    return true;
}

static void memspace_vmem_del_entry(_vmem_table* table, memory_entry_t* entry) {

    vmem_unmap_address_range(table, entry->start, entry->end - entry->start);
}

_vmem_table* memspace_build_vmem(memory_space_t* space) {

    ASSERT(space != NULL);

    memory_entry_t* entry;
    FOR_LLIST(space->del_entries, entry)
        memory_entry_t* curr_entry;
        memory_entry_t* found_entry = NULL;
        FOR_LLIST(space->entries, curr_entry)
            if (found_entry == NULL &&
                entry->start == curr_entry->start &&
                entry->end == curr_entry->end &&
                entry->type == curr_entry->type) {
                    
                found_entry = curr_entry;
            }
        END_FOR_LLIST()
        ASSERT(found_entry != NULL);
        llist_delete_ptr(space->entries, found_entry);
        vfree(found_entry);

        memspace_vmem_del_entry(space->l0_table, entry);
        vfree(entry);
    END_FOR_LLIST()

    FOR_LLIST(space->new_entries, entry)
        switch (entry->type) {
            case MEMSPACE_PHY:
                memspace_vmem_add_phy(space->l0_table, (memory_entry_phy_t*)entry);
                break;
            case MEMSPACE_DEVICE:
                memspace_vmem_add_device(space->l0_table, (memory_entry_device_t*)entry);
                break;
            case MEMSPACE_STACK:
                memspace_vmem_add_stack(space->l0_table, (memory_entry_stack_t*)entry);
                break;
            case MEMSPACE_CACHE:
                memspace_vmem_add_cache(space->l0_table, (memory_entry_cache_t*)entry);
                break;
            default:
                ASSERT(0);
        }

        llist_append_ptr(space->entries, entry);
    END_FOR_LLIST()

    FOR_LLIST(space->update_cache_entries, entry)
        ASSERT(entry->type == MEMSPACE_CACHE);
        memspace_vmem_add_cache(space->l0_table, (memory_entry_cache_t*)entry);
    END_FOR_LLIST();

    llist_free_all(space->new_entries);
    llist_free_all(space->del_entries);
    llist_free_all(space->update_cache_entries);

    space->new_entries = llist_create();
    space->del_entries = llist_create();
    space->update_cache_entries = llist_create();

    return space->l0_table;
}

void memspace_update_cache(memory_space_t* space, memory_entry_cache_t* entry) {
    ASSERT(entry->type == MEMSPACE_CACHE);
    llist_append_ptr(space->update_cache_entries, entry);
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

void* memspace_alloc_entry(void) {
    return vmalloc(sizeof(memory_entry_t));
}

bool memspace_alloc(memory_space_t* space, memory_valloc_ctx_t* ctx) {
    ASSERT(space != NULL);

    space->entries = llist_create();
    space->new_entries = llist_create();
    space->del_entries = llist_create();
    space->update_cache_entries = llist_create();

    space->l0_table = vmem_allocate_empty_table();

    if (ctx != NULL) {
        memcpy(&space->valloc_ctx, ctx, sizeof(memory_valloc_ctx_t));
    } else {
        space->valloc_ctx.systemspace_end = 0;
    }

    return true;
}

void memspace_deallocate(memory_space_t* space) {

    memory_entry_t* entry;
    FOR_LLIST(space->entries, entry)
        vfree(entry);
    END_FOR_LLIST()
    FOR_LLIST(space->new_entries, entry)
        vfree(entry);
    END_FOR_LLIST()
    FOR_LLIST(space->del_entries, entry)
        vfree(entry);
    END_FOR_LLIST()
    FOR_LLIST(space->update_cache_entries, entry)
        vfree(entry);
    END_FOR_LLIST()
    llist_free_all(space->entries);
    llist_free_all(space->new_entries);
    llist_free_all(space->del_entries);
    llist_free_all(space->update_cache_entries);

    vmem_deallocate_table(space->l0_table);
}