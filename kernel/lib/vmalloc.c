
#include <stdint.h>
#include <string.h>

#include "kernel/lib/vmalloc.h"
#include "kernel/kmalloc.h"
#include "kernel/kernelspace.h"
#include "kernel/assert.h"

#include "stdlib/malloc.h"
#include "stdlib/linalloc.h"

//#define DEBUG_VMALLOC

#ifdef DEBUG_VMALLOC


void vmalloc_init(uint64_t size) {
    (void)size;
    return;
}

void* vmalloc(uint64_t size) {

    uint64_t page_size = PAGE_CEIL(size);

    uint64_t mem_phy = (uint64_t)kmalloc_phy(page_size);
    void* mem_virt = memspace_alloc_kernel_virt(page_size, 4096);

    memspace_map_phy_kernel((void*)mem_phy, mem_virt, page_size, 0);
    memspace_update_kernel_vmem();

    memset(mem_virt, 0, page_size);
    return mem_virt;
}

void vfree(void* mem) {
    memory_entry_t* entry = memspace_get_entry_at_addr_kernel(mem);
    ASSERT(entry->type == MEMSPACE_PHY);
    uint64_t mem_phy = ((memory_entry_phy_t*)entry)->phy_addr;
    memspace_unmap_kernel(mem);

    kfree_phy((void*)mem_phy);
}

void vmalloc_calc_stat(malloc_stat_t* stat_out) {
    stat_out->total_mem = 0;
    stat_out->avail_mem = 0;
    stat_out->largest_chunk = 0;
}

#else

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
    void* ret = malloc_p(size, &s_vmalloc_state);
    memset(ret, 0, size);
    return ret;
}

void vfree(void* mem) {
    free_p(mem, &s_vmalloc_state);
}

void vmalloc_calc_stat(malloc_stat_t* stat_out) {
    malloc_calc_stat(&s_vmalloc_state, stat_out);
}

#endif