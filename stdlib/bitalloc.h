
#ifndef __BITALLOC_H__
#define __BITALLOC_H__

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint64_t base;
    uint64_t limit;
    uint64_t* bitfield;
} bitalloc_t;

typedef void* (*bitalloc_alloc_fn)(uint64_t size);
typedef void* (*bitalloc_free_fn)(void* ptr);

void bitalloc_init(bitalloc_t* state, uint64_t base, uint64_t limit, bitalloc_alloc_fn alloc_func);
void bitalloc_deinit(bitalloc_t* state, bitalloc_free_fn free_func);

bool bitalloc_avail(bitalloc_t* state, uint64_t bit);
bool bitalloc_alloc(bitalloc_t* state, uint64_t bit);
bool bitalloc_free(bitalloc_t* state, uint64_t bit);

bool bitalloc_alloc_any(bitalloc_t* state, uint64_t* bit_out);


#endif
