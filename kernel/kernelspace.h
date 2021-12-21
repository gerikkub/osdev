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

void memspace_init_kernelspace(void);
bool memspace_add_entry_to_kernel_memory(memory_entry_t* entry);
_vmem_table* memspace_build_kernel_vmem(void);

void memspace_init_systemspace(void);
memory_space_t* memspace_get_systemspace(void);

void memspace_map_phy_kernel(void* phy_addr, void* virt_addr, uint64_t len, uint32_t flags);
void memspace_map_device_kernel(void* phy_addr, void* virt_addr, uint64_t len, uint32_t flags);
void memspace_update_kernel_vmem(void);

void* get_userspace_ptr(_vmem_table* table_ptr, uintptr_t userptr);

#endif