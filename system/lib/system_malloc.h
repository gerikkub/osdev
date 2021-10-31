
#ifndef __SYSTEM_MALLOC_H__
#define __SYSTEM_MALLOC_H__


void malloc_init(void);

void* malloc(uint64_t size);

void free(void* mem);

#endif
