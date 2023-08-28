
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "kernel/console.h"
#include "kernel/assert.h"
#include "kernel/kernelspace.h"
#include "kernel/lib/libdtb.h"


uintptr_t dt_map_addr_to_phy(dt_node_t* node, uintptr_t addr, bool* valid) {

    if (node->parent == NULL ||
        node->parent->prop_ranges == NULL) {

        *valid = true;
        return addr;
    }

    dt_prop_ranges_t* ranges = node->parent->prop_ranges;

    // Identity map if ranges is present but empty
    if (ranges->num_ranges == 0) {
        return dt_map_addr_to_phy(node->parent, addr, valid);
    }

    uintptr_t child_addr = 0;
    uint64_t child_size = 0;
    uintptr_t parent_addr = 0;

    uint64_t idx;
    for (idx = 0; idx < ranges->num_ranges; idx++) {
        dt_prop_range_entry_t* entry = &ranges->range_entries[idx];

        ASSERT(entry->addr_size > 0);
        ASSERT(entry->paddr_size > 0);
        ASSERT(entry->size_size > 0);
        if (entry->addr_size == 1) {
            child_addr = entry->addr_ptr[0];
        } else {
            child_addr = ((uintptr_t)entry->addr_ptr[entry->addr_size - 2] << 32) | 
                          (uintptr_t)entry->addr_ptr[entry->addr_size - 1];
        }

        if (entry->size_size == 1) {
            child_size = entry->size_ptr[0];
        } else {
            child_size = ((uintptr_t)entry->size_ptr[entry->size_size - 2] << 32) | 
                          (uintptr_t)entry->size_ptr[entry->size_size - 1];
        }

        if (entry->paddr_size == 1) {
            parent_addr = entry->paddr_ptr[0];
        } else {
            parent_addr = ((uintptr_t)entry->paddr_ptr[entry->paddr_size - 2] << 32) | 
                           (uintptr_t)entry->paddr_ptr[entry->paddr_size - 1];
        }

        if (addr >= child_addr &&
            addr < (child_addr + child_size)) {
            break;
        }
    }

    if (idx == ranges->num_ranges) {
        *valid = false;
        return 0;
    } else {
        uint64_t new_addr = (addr - child_addr) + parent_addr;
        return dt_map_addr_to_phy(node->parent, new_addr, valid);
    }
}
