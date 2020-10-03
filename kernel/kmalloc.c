
#include <stdint.h>
#include <stdbool.h>

#include "kernel/assert.h"

#ifdef DEBUG
#include "kernel/console.h"
#endif

#define MEM_SIZE (256*1024*1024)
#define PAGE_SIZE (4*1024)
#define NUM_MEM_BLOCKS (1024)

#define MEMBLOCK_FLAG_ALLOCATED (1 << 0)

#define PAGES(X) (((X) + PAGE_SIZE - 1)/PAGE_SIZE)
#define PAGEMEM(X) (PAGES((X)) * PAGE_SIZE)

typedef struct {
    uint8_t* ptr;
    uint64_t size;
    uint8_t flags;
} memblock_t;

static memblock_t s_memblocks[NUM_MEM_BLOCKS] = {0};

static uint64_t s_last_memblock = 0;

extern uint8_t _data_start;
extern uint8_t _bss_end;
extern uint8_t _heap_base;
extern uint8_t _heap_limit;

//static uintptr_t s_next_ptr;

void kmalloc_init(void) {

    //uint64_t elfmem = (uint64_t)((&_heap_base) - (&_data_start));
    //uint64_t elfpages = PAGES(elfmem);

    //s_memblocks[0].ptr = &_data_start;
    //s_memblocks[0].size = elfpages * PAGE_SIZE;
    //s_memblocks[0].flags = MEMBLOCK_FLAG_ALLOCATED;

    //s_memblocks[1].ptr = (uint8_t*)(((uint64_t)&_data_start) + (elfpages * PAGE_SIZE));
    //s_memblocks[1].size = MEM_SIZE - (elfpages * PAGE_SIZE);
    //s_memblocks[1].flags = 0;

    s_memblocks[0].ptr = &_heap_base;
    s_memblocks[0].size = ((uintptr_t)&_heap_limit) - ((uintptr_t)&_heap_base);
    s_memblocks[0].flags = 0;


    s_last_memblock = 1;

    //s_next_ptr = (uintptr_t)&_heap_base;
}

uint8_t* kmalloc_phy(uint64_t bytes) {

    uint64_t pagebytes = PAGEMEM(bytes);
    uint64_t idx = 0;
    bool found_mem = false;

    /*uintptr_t return_ptr = s_next_ptr;
    s_next_ptr += pagebytes;
    ASSERT(s_next_ptr <= (uint64_t)&_heap_limit);

    return (uint8_t*)return_ptr;*/

    while (!found_mem) {
        if (pagebytes <= s_memblocks[idx].size &&
            !(s_memblocks[idx].flags & MEMBLOCK_FLAG_ALLOCATED)) {
            found_mem = true;
        } else {
            idx++;
        }

        ASSERT(idx <= s_last_memblock)
    }

    if (pagebytes != s_memblocks[idx].size) {
        uint64_t leftover_pagebytes = s_memblocks[idx].size - pagebytes;

        s_last_memblock++;
        ASSERT(s_last_memblock < NUM_MEM_BLOCKS)

        s_memblocks[s_last_memblock].ptr = s_memblocks[idx].ptr + pagebytes;
        s_memblocks[s_last_memblock].size = leftover_pagebytes;
        s_memblocks[s_last_memblock].flags = 0;
    } 

    s_memblocks[idx].flags |= MEMBLOCK_FLAG_ALLOCATED;
    return s_memblocks[idx].ptr;
}

void kfree_phy(uint8_t* ptr) {

    uint64_t idx;
    for (idx = 0; idx < s_last_memblock; idx++) {
        if (s_memblocks[idx].ptr == ptr) {
            break;
        }
    }

    ASSERT(idx < s_last_memblock)

    s_memblocks[idx].flags &= ~(MEMBLOCK_FLAG_ALLOCATED);
}
