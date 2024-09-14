#ifndef __KERNELSPACE_H__
#define __KERNELSPACE_H__

#include <stdint.h>

#include "kernel/memoryspace.h"
#include "kernel/vmem.h"

#define KERNELSPACE_ENTRIES 512
#define SYSTEMSPACE_ENTRIES 512

#define PHY_TO_KSPACE(x) (((uintptr_t)x) | 0xFFFF000000000000UL)
#define KSPACE_TO_PHY(x) (((uintptr_t)x) & 0x0000FFFFFFFFFFFFUL)

#define PHY_TO_KSPACE_PTR(x) ((void*)PHY_TO_KSPACE(x))
#define KSPACE_TO_PHY_PTR(x) ((void*)KSPACE_TO_PHY(x))

#define KSPACE_EXSTACK_SIZE (8192)

#define IS_KSPACE_PTR(x) (((uintptr_t)x) & 0xFFFF000000000000UL)
#define IS_USPACE_PTR(x) (!IS_KSPACE_PTR(x))

#define EARLY_CON_VIRT ((uint8_t*)0xFFFF000009000000UL)
#define EARLY_CON_PHY_BASE 0x8000006000ULL
#define EARLY_PCI_VIRT ((uint8_t*)0xFFFF000009001000UL)
#define EARLY_PCI_PHY_BASE (0x4010010000UL)

void memspace_init_kernelspace(void);
bool memspace_add_entry_to_kernel_memory(memory_entry_t* entry);
void memspace_update_kernel_cache(memory_entry_cache_t* entry);
_vmem_table* memspace_build_kernel_vmem(void);

void memspace_init_systemspace(void);
memory_space_t* memspace_get_systemspace(void);

void memspace_map_phy_kernel(void* phy_addr, void* virt_addr, uint64_t len, uint32_t flags);
void memspace_map_device_kernel(void* phy_addr, void* virt_addr, uint64_t len, uint32_t flags);
void memspace_unmap_kernel(memory_entry_t* entry);

void memspace_update_kernel_vmem(void);

void* memspace_alloc_kernel_virt(uint64_t len, uint64_t align);

memory_entry_t* memspace_get_entry_at_addr_kernel(void* addr_ptr);

void* get_userspace_ptr(_vmem_table* table_ptr, uintptr_t userptr);

bool kspace_vmem_walk_table(uint64_t vmem_addr, uint64_t* phy_addr);

#endif
