#ifndef __KMALLOC_H__
#define __KMALLOC_H__

#include <stdint.h>

void kmalloc_init(void);

uint8_t* kmalloc_phy(uint64_t bytes);

void kfree_phy(uint8_t* ptr);

#endif
