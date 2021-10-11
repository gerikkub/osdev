
#include <stdint.h>

#include "kernel/lib/vmalloc.h"
#include "kernel/kmalloc.h"
#include "kernel/kernelspace.h"
#include "kernel/assert.h"

#include "stdlib/malloc.h"

static malloc_state_t s_vmalloc_state;
static void* s_vmalloc_mem_phy;

void vmalloc_init(uint64_t size) {
    s_vmalloc_mem_phy = kmalloc_phy(size);
    ASSERT(s_vmalloc_mem_phy != NULL);

    malloc_init_p(&s_vmalloc_state, NULL, (void*)PHY_TO_KSPACE(s_vmalloc_mem_phy), size);
}

void* vmalloc(uint64_t size) {
    return malloc_p(size, &s_vmalloc_state);
}

void vfree(void* mem) {
    free_p(mem, &s_vmalloc_state);
}