#ifndef __KMALLOC_H__
#define __KMALLOC_H__

#include <stdint.h>

void kmalloc_init(void);

void* kmalloc_phy(uint64_t bytes);

void kfree_phy(void* ptr);

#endif
