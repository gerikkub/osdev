
#include <stdint.h>

#include "kernel/kernelspace.h"
#include "kernel/memoryspace.h"
#include "kernel/kmalloc.h"
#include "kernel/assert.h"

static memory_space_t s_kernelspace;

extern uint8_t _text_start;
extern uint8_t _text_end;
extern uint8_t _data_start;
extern uint8_t _data_end;
extern uint8_t _bss_start;
extern uint8_t _bss_end;

void memspace_init_kernelspace(void) {

    bool result;

    memory_entry_t* kentries = (memory_entry_t*)kmalloc_phy(KERNELSPACE_ENTRIES * sizeof(memory_entry_t));

    s_kernelspace.entries = kentries;
    s_kernelspace.num = 0;
    s_kernelspace.maxnum = KERNELSPACE_ENTRIES;

    memory_entry_phy_t text_entry = {
        .start = ((uint64_t)&_text_start) & 0xFFFFFFFFFFFF,
        .end = (((uint64_t)&_text_end) - 1) & 0xFFFFFFFFFFFF,
        .type = MEMSPACE_PHY,
        .flags = MEMSPACE_FLAG_PERM_KRE,
        .phy_addr = ((uint64_t)&_text_start) & 0xFFFFFFFFFFFF
    };

    result = memspace_add_entry_to_memory(&s_kernelspace, (memory_entry_t*)&text_entry);
    ASSERT(result);

    if (((addr_phy_t)&_data_start) != ((addr_phy_t)&_data_end)) {
        memory_entry_phy_t data_entry = {
            .start = ((uint64_t)&_data_start) & 0xFFFFFFFFFFFF,
            .end = (((uint64_t)&_data_end) - 1) & 0xFFFFFFFFFFFF,
            .type = MEMSPACE_PHY,
            .flags = MEMSPACE_FLAG_PERM_KRW,
            .phy_addr = ((uint64_t)&_data_start) & 0xFFFFFFFFFFFF
        };

        result = memspace_add_entry_to_memory(&s_kernelspace, (memory_entry_t*)&data_entry);
        ASSERT(result);
    }

    memory_entry_phy_t bss_entry = {
        .start = ((uint64_t)&_bss_start) & 0xFFFFFFFFFFFF,
        .end = (((uint64_t)&_bss_end) - 1) & 0xFFFFFFFFFFFF,
        .type = MEMSPACE_PHY,
        .flags = MEMSPACE_FLAG_PERM_KRW,
        .phy_addr = ((uint64_t)&_bss_start) & 0xFFFFFFFFFFFF
    };

    result = memspace_add_entry_to_memory(&s_kernelspace, (memory_entry_t*)&bss_entry);
    ASSERT(result);
}

bool memspace_add_entry_to_kernel_memory(memory_entry_t* entry) {
    return memspace_add_entry_to_memory(&s_kernelspace, entry);
}

_vmem_table* memspace_build_kernel_vmem(void) {
    return memspace_build_vmem(&s_kernelspace);
}