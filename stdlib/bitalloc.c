
#include <stdint.h>
#include <stdbool.h>

#ifdef KERNEL_BUILD
#include "kernel/assert.h"
#define SYS_ASSERT(x) ASSERT(x)
#else
#include "system/lib/system_assert.h"
#endif


#include "stdlib/bitutils.h"
#include "stdlib/bitalloc.h"
#include "stdlib/string.h"

void bitalloc_init(bitalloc_t* state, uint64_t base, uint64_t limit, bitalloc_alloc_fn alloc_func) {

    SYS_ASSERT(state != NULL);
    SYS_ASSERT(alloc_func != NULL);

    uint64_t size = (limit - base + 63) / 64;

    state->base = base;
    state->limit = limit;
    state->bitfield = alloc_func(size * sizeof(uint64_t));

    memset(state->bitfield, 0, size * sizeof(uint64_t));
}

void bitalloc_deinit(bitalloc_t* state, bitalloc_free_fn free_func) {
    SYS_ASSERT(state != NULL);
    SYS_ASSERT(state->bitfield != NULL);

    free_func(state->bitfield);
}

static void bitalloc_clearbit(bitalloc_t* state, uint64_t bit) {
    SYS_ASSERT(bit >= state->base);
    SYS_ASSERT(bit < state->limit);

    const uint64_t bit_num = bit - state->base;
    const uint64_t bit_div = bit_num / (sizeof(uint64_t) * 8);
    const uint64_t bit_mod = bit_num % (sizeof(uint64_t) * 8);

    state->bitfield[bit_div] &= ~(1UL << bit_mod);
}

static void bitalloc_setbit(bitalloc_t* state, uint64_t bit) {
    SYS_ASSERT(bit >= state->base);
    SYS_ASSERT(bit < state->limit);

    const uint64_t bit_num = bit - state->base;
    const uint64_t bit_div = bit_num / (sizeof(uint64_t) * 8);
    const uint64_t bit_mod = bit_num % (sizeof(uint64_t) * 8);

    state->bitfield[bit_div] |= (1UL << bit_mod);
}

bool bitalloc_avail(bitalloc_t* state, uint64_t bit) {
    SYS_ASSERT(bit >= state->base);
    SYS_ASSERT(bit < state->limit);

    const uint64_t bit_num = bit - state->base;
    const uint64_t bit_div = bit_num / (sizeof(uint64_t) * 8);
    const uint64_t bit_mod = bit_num % (sizeof(uint64_t) * 8);

    return (state->bitfield[bit_div] & (1UL << bit_mod)) != 0;
}

bool bitalloc_alloc(bitalloc_t* state, uint64_t bit) {
    if (bitalloc_avail(state, bit)) {
        bitalloc_setbit(state, bit);
        return true;
    } else {
        return false;
    }
}

bool bitalloc_free(bitalloc_t* state, uint64_t bit) {
    if (!bitalloc_avail(state, bit)) {
        bitalloc_clearbit(state, bit);
        return true;
    } else {
        return false;
    }
}

bool bitalloc_alloc_any(bitalloc_t* state, uint64_t* bit_out) {

    uint64_t idx;
    
    for (idx = 0; idx < (state->base - state->limit); idx++) {
        if (state->bitfield[idx] != 0xFFFFFFFFFFFFFFFFUL) {
            break;
        }
    }

    if (idx == (state->base - state->limit)) {
        return false;
    } else {
        uint64_t bitidx;
        for (bitidx = 0; bitidx < 64; bitidx++) {
            if (!(state->bitfield[idx] & (1UL << bitidx))) {
                *bit_out = idx * 64 + bitidx + state->base;
                state->bitfield[idx] |= 1UL << bitidx;
                return true;
            }
        }

        SYS_ASSERT(bitidx < 64);
        return false;
    }
}