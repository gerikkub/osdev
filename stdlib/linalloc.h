
#ifndef __LINALLOC_H__
#define __LINALLOC_H__

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint64_t base;
    uint64_t limit;
    uint64_t align;
} linalloc_t;


void linalloc_init(linalloc_t* state, uint64_t base, uint64_t limit, uint64_t align);
uint64_t linalloc_alloc(linalloc_t* state, uint64_t size, bool* valid);


#endif