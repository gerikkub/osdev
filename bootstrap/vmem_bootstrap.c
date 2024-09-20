
#include <stdint.h>
#include <stdbool.h>

#include "bootstrap/vmem_bootstrap.h"




_vmem_entry_t vmem_entry_block_bootstrap(uint16_t attr_hi, uint16_t attr_lo, uintptr_t addr, uint8_t block_level) {

    if (block_level == 1) {
        return ((_vmem_entry_block_t)attr_hi) |
               ((_vmem_entry_block_t)attr_lo) |
               (((_vmem_entry_block_t)addr) & 0xFFFFC0000000) |
               1;
    } else if (block_level == 2) {
        return ((_vmem_entry_block_t)attr_hi) |
               ((_vmem_entry_block_t)attr_lo) |
               (((_vmem_entry_block_t)addr) & 0xFFFFFFE00000) |
               1;
    } else {
        return 0;
    }
}

_vmem_entry_t vmem_entry_table_bootstrap(uintptr_t addr, uint64_t flags) {

    return ((_vmem_entry_table_t)flags) |
           (((_vmem_entry_table_t)addr) & 0xFFFFFFFFF000) |
           3;
}


