
#ifndef __SYSTEM_MALLOC_H__
#define __SYSTEM_MALLOC_H__

#include <stdlib.h>

void malloc_init();

void* malloc(uint64_t size);

void free(void* mem);

#endif