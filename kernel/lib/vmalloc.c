
#include <stdint.h>

#include "kernel/lib/vmalloc.h"
#include "kernel/kmalloc.h"
#include "kernel/kernelspace.h"
#include "kernel/assert.h"

#include "stdlib/malloc.h"

typedef struct {
    uintptr_t mem;
    uint64_t len;
} vmalloc_ctx_t;

static malloc_state_t s_vmalloc_state;
static void* s_vmalloc_mem_phy;
static vmalloc_ctx_t s_vmalloc_ctx;


int64_t vmalloc_add_mem(bool initialize, uint64_t req_size, void* ctx, malloc_state_t* state) {
    if (initialize) {
        vmalloc_ctx_t* vmalloc_ctx = ctx;
        state->base = vmalloc_ctx->mem;
        state->limit = state->base + vmalloc_ctx->len;
        return vmalloc_ctx->len;
    } else {
        return 0;
    }
}

void vmalloc_init(uint64_t size) {
    s_vmalloc_mem_phy = kmalloc_phy(size);
    ASSERT(s_vmalloc_mem_phy != NULL);
    
    s_vmalloc_ctx.mem = PHY_TO_KSPACE(s_vmalloc_mem_phy);
    s_vmalloc_ctx.len = size;

    malloc_init_p(&s_vmalloc_state, vmalloc_add_mem, &s_vmalloc_ctx);
}

void* vmalloc(uint64_t size) {
    return malloc_p(size, &s_vmalloc_state);
}

void vfree(void* mem) {
    // free_p(mem, &s_vmalloc_state);
}