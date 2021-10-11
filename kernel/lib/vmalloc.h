
#ifndef __VMALLOC_H__
#define __VMALLOC_H__

#include <stdint.h>

void vmalloc_init(uint64_t size);

void* vmalloc(uint64_t size);

void vfree(void* mem);


#endif