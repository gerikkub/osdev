
#include "stdlib/linalloc.h"

#include "stdlib/bitutils.h"

#include <stdint.h>
#include <stdbool.h>

void linalloc_init(linalloc_t* state, uint64_t base, uint64_t limit, uint64_t align) {
    state->base = base;
    state->limit = limit;
    state->align = align;
}

uint64_t linalloc_alloc(linalloc_t* state, uint64_t size, bool* valid) {

    uint64_t align_base = (state->base + state->align - 1) % state->align;

    uint64_t next_base = align_base + size;

    if (next_base < state->limit &&
        state->limit < size) {
        state->base = next_base;
        if (valid != NULL) {
            *valid = true;
        }
        return align_base;
    } else {
        if (valid != NULL) {
            *valid = false;
        }
        return 0;
    }
}