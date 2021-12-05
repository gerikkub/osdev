
#include <stdint.h>
#include <stdbool.h>

#include "kernel/kernelspace.h"
#include "kernel/memoryspace.h"
#include "kernel/kmalloc.h"
#include "kernel/assert.h"

static memory_space_t s_kernelspace;
static memory_space_t s_systemspace;

extern uint8_t _text_start;
extern uint8_t _text_end;
extern uint8_t _data_start;
extern uint8_t _data_end;
extern uint8_t _bss_start;
extern uint8_t _bss_end;
extern uint8_t _systemspace_text_start __attribute__((section(".systemspace.text")));
extern uint8_t _systemspace_text_end;
extern uint8_t _systemspace_data_start;
extern uint8_t _systemspace_data_end;
extern uint8_t _systemspace_bss_start;
extern uint8_t _systemspace_bss_end;

void memspace_init_kernelspace(void) {

    bool result;

    void* kentries_phy = kmalloc_phy(KERNELSPACE_ENTRIES * sizeof(memory_entry_t));
    ASSERT(kentries_phy != NULL);
    memory_entry_t* kentries = (memory_entry_t*)PHY_TO_KSPACE(kentries_phy);

    s_kernelspace.entries = kentries;
    s_kernelspace.num = 0;
    s_kernelspace.maxnum = KERNELSPACE_ENTRIES;

    memory_entry_phy_t text_entry = {
        .start = ((uint64_t)&_text_start) & 0xFFFFFFFFFFFF,
        .end = ((uint64_t)&_text_end) & 0xFFFFFFFFFFFF,
        .type = MEMSPACE_PHY,
        .flags = MEMSPACE_FLAG_PERM_KRE,
        .phy_addr = ((uint64_t)&_text_start) & 0xFFFFFFFFFFFF
    };

    result = memspace_add_entry_to_memory(&s_kernelspace, (memory_entry_t*)&text_entry);
    ASSERT(result);

    if (((addr_phy_t)&_data_start) != ((addr_phy_t)&_data_end)) {
        memory_entry_phy_t data_entry = {
            .start = ((uint64_t)&_data_start) & 0xFFFFFFFFFFFF,
            .end = ((uint64_t)&_data_end) & 0xFFFFFFFFFFFF,
            .type = MEMSPACE_PHY,
            .flags = MEMSPACE_FLAG_PERM_KRW,
            .phy_addr = ((uint64_t)&_data_start) & 0xFFFFFFFFFFFF
        };

        result = memspace_add_entry_to_memory(&s_kernelspace, (memory_entry_t*)&data_entry);
        ASSERT(result);
    }

    memory_entry_phy_t bss_entry = {
        .start = ((uint64_t)&_bss_start) & 0xFFFFFFFFFFFF,
        .end = ((uint64_t)&_bss_end) & 0xFFFFFFFFFFFF,
        .type = MEMSPACE_PHY,
        .flags = MEMSPACE_FLAG_PERM_KRW,
        .phy_addr = ((uint64_t)&_bss_start) & 0xFFFFFFFFFFFF
    };

    result = memspace_add_entry_to_memory(&s_kernelspace, (memory_entry_t*)&bss_entry);
    ASSERT(result);
}

void memspace_init_systemspace(void) {

    uintptr_t ss_text_start, ss_text_end, ss_data_start, ss_data_end;
    GET_ABS_SYM(ss_text_start, _systemspace_text_start);
    GET_ABS_SYM(ss_text_end, _systemspace_text_end);
    GET_ABS_SYM(ss_data_start, _systemspace_data_start);
    GET_ABS_SYM(ss_data_end, _systemspace_data_end);
    

    /*bool result;

    memory_entry_t* sysentries = (memory_entry_t*)kmalloc_phy(SYSTEMSPACE_ENTRIES * sizeof(memory_entry_t));

    s_systemspace.entries = sysentries;
    s_systemspace.num = 0;
    s_systemspace.maxnum = SYSTEMSPACE_ENTRIES;

    memory_entry_phy_t text_entry = {
        .start = ((uint64_t)&_systemspace_text_start),
        .end = ((uint64_t)&_systemspace_text_end) - 1,
        .type = MEMSPACE_PHY,
        .flags = MEMSPACE_FLAG_PERM_URE,
        .phy_addr = ((uint64_t)&_systemspace_text_start)
    };

    result = memspace_add_entry_to_memory(&s_systemspace, (memory_entry_t*)&text_entry);
    ASSERT(result);

    if (((addr_phy_t)&_systemspace_data_start) != ((addr_phy_t)&_systemspace_data_end)) {
        memory_entry_phy_t data_entry = {
            .start = ((uint64_t)&_systemspace_data_start),
            .end = ((uint64_t)&_systemspace_data_end) - 1,
            .type = MEMSPACE_PHY,
            .flags = MEMSPACE_FLAG_PERM_URW,
            .phy_addr = ((uint64_t)&_systemspace_data_start)
        };

        result = memspace_add_entry_to_memory(&s_systemspace, (memory_entry_t*)&data_entry);
        ASSERT(result);
    }

    if (((addr_phy_t)&_systemspace_bss_start) != ((addr_phy_t)&_systemspace_bss_end)) {
        memory_entry_phy_t bss_entry = {
            .start = ((uint64_t)&_systemspace_bss_start),
            .end = ((uint64_t)&_systemspace_bss_end) - 1,
            .type = MEMSPACE_PHY,
            .flags = MEMSPACE_FLAG_PERM_URW,
            .phy_addr = ((uint64_t)&_systemspace_bss_start)
        };

        result = memspace_add_entry_to_memory(&s_systemspace, (memory_entry_t*)&bss_entry);
        ASSERT(result);
    }*/
}

bool memspace_add_entry_to_kernel_memory(memory_entry_t* entry) {
    return memspace_add_entry_to_memory(&s_kernelspace, entry);
}

_vmem_table* memspace_build_kernel_vmem(void) {
    return memspace_build_vmem(&s_kernelspace);
}

memory_space_t* memspace_get_systemspace(void) {
    return &s_systemspace;
}

void memspace_map_phy_kernel(void* phy_addr, void* virt_addr, uint64_t len, uint32_t flags) {

    memory_entry_phy_t entry = {
        .start = (uintptr_t)virt_addr,
        .end = ((uintptr_t)virt_addr) + len,
        .type = MEMSPACE_PHY,
        .flags = flags,
        .phy_addr = (uintptr_t)phy_addr,
        .kmem_addr = (uintptr_t)virt_addr
    };

    memspace_add_entry_to_kernel_memory((memory_entry_t*)&entry);
}

void memspace_map_device_kernel(void* phy_addr, void* virt_addr, uint64_t len, uint32_t flags) {

    memory_entry_device_t entry = {
        .start = (uintptr_t)virt_addr,
        .end = ((uintptr_t)virt_addr) + len,
        .type = MEMSPACE_DEVICE,
        .flags = flags,
        .phy_addr = (uintptr_t)phy_addr
    };

    memspace_add_entry_to_kernel_memory((memory_entry_t*)&entry);
}

void memspace_update_kernel_vmem(void) {

    _vmem_table* kernel_vmem_table = memspace_build_kernel_vmem();

    vmem_set_kernel_table(kernel_vmem_table);
    vmem_flush_tlb();
}