#ifndef __MEMORYSPACE_H__
#define __MEMORYSPACE_H__

#include <stdint.h>
#include <stdbool.h>

#include "kernel/vmem.h"
#include "kernel/lib/llist.h"
#include "stdlib/bitutils.h"

typedef enum {
    MEMSPACE_PHY,
    MEMSPACE_DEVICE,
    MEMSPACE_STACK,
    MEMSPACE_CACHE
} memspace_type_t;

typedef struct __attribute__((packed)) {
    uint64_t start;
    uint64_t end;
    uint32_t type;
    uint32_t flags;
    uint64_t callsite;
    uint64_t args[4];
} memory_entry_t;

// MEMSPACE_PHY
// Addresses are mapped to a physical address range
typedef struct __attribute__((packed)) {
    uint64_t start;     // VMEM allocated start
    uint64_t end;       // VMEM allocated end
    uint32_t type;      // MEMSPACE_PHY
    uint32_t flags;     // Permissions
    uint64_t callsite;  // Pointer to the callsite that allocated this object
    uint64_t phy_addr;  // Physical address of the page range
    uint64_t kmem_addr; // Address of the page in kernel space
    uint64_t res[2];
} memory_entry_phy_t;

// MEMSPACE_DEVICE
// Addresses mapped to device memory
typedef struct __attribute__((packed)) {
    uint64_t start;     // VMEM allocated start
    uint64_t end;       // VMEM allocated end
    uint32_t type;      // MEMSPACE_DEVICE
    uint32_t flags;     // Permissions
    uint64_t callsite;  // Pointer to the callsite that allocated this object
    uint64_t phy_addr;  // Physical address of the page range
    uint64_t res[3];
} memory_entry_device_t;

// MEMSPACE_STACK
// Addresses used for stack spaces. May be grown dynamically
// TODO: Need to handle physical addresses in a way that can
//       be grown
typedef struct __attribute__((packed)) {
    uint64_t start;      // VMEM allocated start
    uint64_t end;        // VMEM allocated end
    uint32_t type;       // MEMSPACE_STACK
    uint32_t flags;      // Permissions. Execute permission not allowed in stack space
    uint64_t callsite;  // Pointer to the callsite that allocated this object
    uint64_t phy_addr;   // Physical address of the page range
    uint64_t base;       // Stack base address. Addresses above this are reserved for guard pages
    uint64_t limit;      // Current limit address of physically allocated stack space
    uint64_t maxlimit;   // Max limit for stack space. Addresses below this are reserved for guard pages
} memory_entry_stack_t;


typedef struct {
    uint64_t offset;
    uintptr_t phy_addr;
    uint64_t len;
} memcache_phy_entry_t;

typedef bool (*cacheop_populate_virt_fn)(void* ctx, uintptr_t addr, memcache_phy_entry_t* new_entry);
typedef struct {
    cacheop_populate_virt_fn populate_virt_fn;
} memcache_ops_t;

// MEMSPACE_CACHE
// Continuous virtual memory spaces that are not necessary
// to back at all times. May be read lazily with cache maintanence
// instructions
typedef struct __attribute__((packed)) {
    uint64_t start;      // VMEM allocated start
    uint64_t end;        // VMEM allocated end
    uint32_t type;       // MEMSPACE_STACK
    uint32_t flags;      // Permissions. Execute permission not allowed in stack space
    uint64_t callsite;  // Pointer to the callsite that allocated this object
    llist_t* phy_addr_list; // llist of vmem to phy mappings
    memcache_ops_t* cacheops_ptr; // Pointer to a cache maintenance call
    void* cacheops_ctx; // Context to pass in cache maintenance calls
    uint64_t res[1];
} memory_entry_cache_t;

typedef struct {
    uint64_t systemspace_end;
} memory_valloc_ctx_t;

typedef struct {
    llist_head_t entries;
    llist_head_t new_entries;
    llist_head_t del_entries;
    llist_head_t update_cache_entries;
    _vmem_table* l0_table;
    memory_valloc_ctx_t valloc_ctx;
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
void memspace_remove_entry_from_memory(memory_space_t* space, memory_entry_t* entry);
_vmem_table* memspace_build_vmem(memory_space_t* space);
void memspace_update_cache(memory_space_t* space, memory_entry_cache_t* entry);
bool memspace_alloc_space(memory_space_t* space, uint64_t len, memory_entry_t* entry_out);
void* memspace_alloc_entry(void);
bool memspace_alloc(memory_space_t* space, memory_valloc_ctx_t* ctx);
void memspace_deallocate(memory_space_t* space);

#endif