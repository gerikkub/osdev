#ifndef __KERNELSPACE_H__
#define __KERNELSPACE_H__

#include <stdint.h>

#include "kernel/memoryspace.h"
#include "kernel/vmem.h"

#define KERNELSPACE_ENTRIES 512

void memspace_init_kernelspace(void);
bool memspace_add_entry_to_kernel_memory(memory_entry_t* entry);
_vmem_table* memspace_build_kernel_vmem(void);


#endif