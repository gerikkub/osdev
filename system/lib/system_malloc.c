
#include <stdint.h>

#include "stdlib/malloc.h"
#include "stdlib/bitutils.h"

#include "system/lib/system_lib.h"
#include "system/lib/system_assert.h"

#include "include/k_syscall.h"

#define MIN_MALLOC_SBRK_SIZE 32768

static malloc_state_t s_malloc_state;

static int64_t malloc_add_mem(bool initialize, uint64_t len, void* ctx, malloc_state_t* state) {

    if (initialize) {
        uintptr_t base;
        uintptr_t limit;
        SYSCALL_CALL_RET(SYSCALL_SBRK, 0, 0, 0, 0, base);
        SYS_ASSERT(base > 0);

        SYSCALL_CALL_RET(SYSCALL_SBRK, MIN_MALLOC_SBRK_SIZE, 0, 0, 0, limit);
        SYS_ASSERT(limit > 0);

        state->base = base;
        state->limit = limit;
        return MIN_MALLOC_SBRK_SIZE;
    } else {
        uintptr_t limit;

        SYSCALL_CALL_RET(SYSCALL_SBRK, len, 0, 0, 0, limit);
        SYS_ASSERT(limit > 0);

        state->limit = limit;
        return len;
    }
}

void malloc_init(void) {
    malloc_init_p(&s_malloc_state, malloc_add_mem, NULL);
}

void* malloc(uint64_t size) {
    return malloc_p(size, &s_malloc_state);
}

void free(void* mem) {
    free_p(mem, &s_malloc_state);
}