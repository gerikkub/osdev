#ifndef __MEMORYSPACE_H__
#define __MEMORYSPACE_H__

#include <stdint.h>
#include <stdbool.h>

#include "kernel/vmem.h"
#include "kernel/bitutils.h"

typedef enum {
    MEMSPACE_PHY,
    MEMSPACE_DEVICE
} memspace_type_t;

typedef struct __attribute__((packed)) {
    uint64_t start;
    uint64_t end;
    uint32_t type;
    uint32_t flags;
    uint64_t args[5];
} memory_entry_t;

typedef struct __attribute__((packed)) {
    uint64_t start;
    uint64_t end;
    uint32_t type;
    uint32_t flags;
    uint64_t phy_addr;
    uint64_t kmem_addr;
    uint64_t res[2];
} memory_entry_phy_t;

typedef struct __attribute__((packed)) {
    uint64_t start;
    uint64_t end;
    uint32_t type;
    uint32_t flags;
    uint64_t phy_addr;
    uint64_t res[3];
} memory_entry_device_t;

typedef struct {
    memory_entry_t* entries;
    uint64_t num;
    uint64_t maxnum;
} memory_space_t;

#define MEMSPACE_FLAG_PERM_MASK (0x7)
// User Read Only. Kernel Read Only
#define MEMSPACE_FLAG_PERM_URO (0)
// User Read Write. Kernel Read Write
#define MEMSPACE_FLAG_PERM_URW (1)
// User Read Execute. Kernel Read Only
#define MEMSPACE_FLAG_PERM_URE (2)
// User No Permission. Kernel Read Only
#define MEMSPACE_FLAG_PERM_KRO (3)
// User No Permission. Kernel Read Write
#define MEMSPACE_FLAG_PERM_KRW (4)
// User No Permission. Kernel Read Execute
#define MEMSPACE_FLAG_PERM_KRE (5)

memory_entry_t* memspace_get_entry_at_addr(memory_space_t* space, void* addr_ptr);
bool memspace_add_entry_to_memory(memory_space_t* space, memory_entry_t* entry);
_vmem_table* memspace_build_vmem(memory_space_t* space);


#endif