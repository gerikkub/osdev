
#include <stdint.h>
#include <stdbool.h>

#include "kernel/kernelspace.h"
#include "kernel/memoryspace.h"
#include "kernel/kmalloc.h"
#include "kernel/assert.h"
#include "kernel/lib/vmalloc.h"

static memory_space_t s_kernelspace;
static memory_space_t s_systemspace;
static _vmem_table* s_kernelspace_vmem;

// Hold the highest virtual address in use
static uintptr_t s_kernelspace_virttop;

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

    result = memspace_alloc(&s_kernelspace, NULL);
    ASSERT(result);

    memory_entry_phy_t* text_entry = memspace_alloc_entry();
    text_entry->start = ((uint64_t)&_text_start);
    text_entry->end = ((uint64_t)&_text_end);
    text_entry->type = MEMSPACE_PHY;
    text_entry->flags = MEMSPACE_FLAG_PERM_KRE;
    text_entry->phy_addr = ((uint64_t)&_text_start) & 0xFFFFFFFFFFFF;

    result = memspace_add_entry_to_memory(&s_kernelspace, (memory_entry_t*)text_entry);
    ASSERT(result);

    if (((addr_phy_t)&_data_start) != ((addr_phy_t)&_data_end)) {
        memory_entry_phy_t* data_entry = memspace_alloc_entry();
        data_entry->start = ((uint64_t)&_data_start);
        data_entry->end = ((uint64_t)&_data_end);
        data_entry->type = MEMSPACE_PHY;
        data_entry->flags = MEMSPACE_FLAG_PERM_KRW;
        data_entry->phy_addr = ((uint64_t)&_data_start) & 0xFFFFFFFFFFFF;

        result = memspace_add_entry_to_memory(&s_kernelspace, (memory_entry_t*)data_entry);
        ASSERT(result);
    }

    memory_entry_phy_t* bss_entry = memspace_alloc_entry();
    bss_entry->start = ((uint64_t)&_bss_start);
    bss_entry->end = ((uint64_t)&_bss_end);
    bss_entry->type = MEMSPACE_PHY;
    bss_entry->flags = MEMSPACE_FLAG_PERM_KRW;
    bss_entry->phy_addr = ((uint64_t)&_bss_start) & 0xFFFFFFFFFFFF;

    result = memspace_add_entry_to_memory(&s_kernelspace, (memory_entry_t*)bss_entry);
    ASSERT(result);

    s_kernelspace_virttop = PHY_TO_KSPACE(256ULL * 1024 * 1024 * 1024);
    s_kernelspace_vmem = NULL;
}

void memspace_init_systemspace(void) {

    uintptr_t ss_text_start, ss_text_end, ss_data_start, ss_data_end;
    GET_ABS_SYM(ss_text_start, _systemspace_text_start);
    GET_ABS_SYM(ss_text_end, _systemspace_text_end);
    GET_ABS_SYM(ss_data_start, _systemspace_data_start);
    GET_ABS_SYM(ss_data_end, _systemspace_data_end);
}

bool memspace_add_entry_to_kernel_memory(memory_entry_t* entry) {
    return memspace_add_entry_to_memory(&s_kernelspace, entry);
}

void memspace_update_kernel_cache(memory_entry_cache_t* entry) {
    memspace_update_cache(&s_kernelspace, entry);
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

void memspace_unmap_kernel(memory_entry_t* entry) {
    memspace_remove_entry_from_memory(&s_kernelspace, entry);
}

void memspace_update_kernel_vmem(void) {

    _vmem_table* kernel_vmem_table = memspace_build_kernel_vmem();

    vmem_set_kernel_table(kernel_vmem_table);
    vmem_flush_tlb();

    s_kernelspace_vmem = kernel_vmem_table;
}

void* memspace_alloc_kernel_virt(uint64_t len, uint64_t align) {
    if (align < VMEM_PAGE_SIZE) {
        align = VMEM_PAGE_SIZE;
    }
    uintptr_t alloc_base = (s_kernelspace_virttop + (align - 1)) & (~(align - 1));
    uintptr_t len_page = PAGE_CEIL(len);
    s_kernelspace_virttop = alloc_base + len_page;
    return (void*)alloc_base;
}

memory_entry_t* memspace_get_entry_at_addr_kernel(void* addr_ptr) {
    return memspace_get_entry_at_addr(&s_kernelspace, addr_ptr);
}

void* get_userspace_ptr(_vmem_table* table_ptr, uintptr_t userptr) {
    bool walk_ok;
    uint64_t phy_addr;
    walk_ok = vmem_walk_table(table_ptr, userptr, &phy_addr);
    if (!walk_ok) {
        return NULL;
    }
    return PHY_TO_KSPACE_PTR(phy_addr);
}