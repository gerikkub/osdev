#ifndef __KMALLOC_H__
#define __KMALLOC_H__

#include <stdint.h>

typedef struct {
    uintptr_t start;
    uintptr_t end;
} mask_range_t;

void kmalloc_init(void);

void* kmalloc_phy(uint64_t bytes);

void kfree_phy(void* ptr);

void print_kmalloc_debug(uint64_t alloc_size);

void discover_phy_mem_dtb(mask_range_t* mask_ranges, uint64_t num_ranges);

#endif
