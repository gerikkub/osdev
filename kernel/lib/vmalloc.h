
#ifndef __VMALLOC_H__
#define __VMALLOC_H__

#include <stdint.h>

#include <stdlib/malloc.h>

void vmalloc_init(uint64_t size);

void* vmalloc(uint64_t size);

void vfree(const void* mem);

void vmalloc_calc_stat(malloc_stat_t* stat_out);
void vmalloc_check_structure(void);

#endif