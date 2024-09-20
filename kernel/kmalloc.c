
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "kernel/assert.h"
#include "kernel/kmalloc.h"
#include "kernel/kernelspace.h"
#include "kernel/dtb.h"

#include "stdlib/bitutils.h"

#include "kernel/console.h"

#define MEM_SIZE (1024*1024*1024)
#define PAGE_SIZE (4*1024)
#define NUM_MEM_BLOCKS (4096*256)

#define MEMBLOCK_FLAG_ALLOCATED (1 << 0)
#define MEMBLOCK_MAGIC (0xABAB1234)

#define PAGES(X) (((X) + PAGE_SIZE - 1)/PAGE_SIZE)
#define PAGEMEM(X) (PAGES((X)) * PAGE_SIZE)

typedef struct {
    uintptr_t ptr;
    uint64_t size;
    uint64_t magic;
    uint8_t flags;
} memblock_t;

static memblock_t s_memblocks[NUM_MEM_BLOCKS] = {0};

static uint64_t s_last_memblock = 0;

extern uint8_t _data_start;
extern uint8_t _bss_end;
extern uint8_t _heap_base;
extern uint8_t _heap_limit;

static uint64_t kmalloc_op_num = 0;

void kmalloc_add_phy_memory(uintptr_t base_phy, uintptr_t len) {

    ASSERT((len & (PAGE_SIZE-1)) == 0);

    console_log(LOG_DEBUG, "Adding PHY memory [%16x, %16x] (%d KB)",
                base_phy, base_phy + len, len / 1024);

    s_last_memblock++;
    ASSERT(s_last_memblock < NUM_MEM_BLOCKS);

    s_memblocks[s_last_memblock].ptr = base_phy;
    s_memblocks[s_last_memblock].size = len;
    s_memblocks[s_last_memblock].magic = MEMBLOCK_MAGIC;
    s_memblocks[s_last_memblock].flags = 0;
}

void kmalloc_init(void) {

    uintptr_t heap_base_phy = KSPACE_TO_PHY(&_heap_base);
    uintptr_t heap_limit_phy = KSPACE_TO_PHY(&_heap_limit);

    s_memblocks[0].ptr = heap_base_phy;
    s_memblocks[0].size = heap_limit_phy - heap_base_phy;
    s_memblocks[0].magic = MEMBLOCK_MAGIC;
    s_memblocks[0].flags = 0;


    s_last_memblock = 0;
}

void kmalloc_check_structure(void) {

    uint64_t idx;
    //uintptr_t exp_ptr = KSPACE_TO_PHY(&_heap_base);
 
    for (idx = 0; idx <= s_last_memblock; idx++) {
        memblock_t* this_block = &s_memblocks[idx];
        ASSERT(this_block->magic == MEMBLOCK_MAGIC);
        //ASSERT(exp_ptr == this_block->ptr);

        //exp_ptr += this_block->size;
        //uintptr_t heap_limit_phy = KSPACE_TO_PHY(&_heap_limit);
        //ASSERT(exp_ptr <= heap_limit_phy);
    }
}

void* kmalloc_phy(uint64_t bytes) {

    uint64_t pagebytes = PAGEMEM(bytes);
    uint64_t idx = 0;
    bool found_mem = false;

    kmalloc_op_num++;
    //console_log(LOG_DEBUG, "kmalloc_phy: %u", kmalloc_op_num);

    while (!found_mem) {
        if (pagebytes <= s_memblocks[idx].size &&
            !(s_memblocks[idx].flags & MEMBLOCK_FLAG_ALLOCATED)) {
            found_mem = true;
        } else {
            idx++;
        }

        if (idx > s_last_memblock) {
            print_kmalloc_debug(pagebytes);
        }
        ASSERT(idx <= s_last_memblock)
    }

    if (pagebytes != s_memblocks[idx].size) {
        uint64_t leftover_pagebytes = s_memblocks[idx].size - pagebytes;

        s_memblocks[idx].size = pagebytes;

        memblock_t new_block = {
            .ptr = s_memblocks[idx].ptr + pagebytes,
            .size = leftover_pagebytes,
            .magic = MEMBLOCK_MAGIC,
            .flags = 0
        };

        s_last_memblock++;
        ASSERT(s_last_memblock < NUM_MEM_BLOCKS)

        uint64_t shift_idx = s_last_memblock;

        while (shift_idx > idx) {
            s_memblocks[shift_idx] = s_memblocks[shift_idx-1];
            shift_idx--;
        }
        s_memblocks[idx+1] = new_block;
    } 

    s_memblocks[idx].flags |= MEMBLOCK_FLAG_ALLOCATED;
    MEM_DMB();

    kmalloc_check_structure();

    memset(PHY_TO_KSPACE_PTR(s_memblocks[idx].ptr), 0, pagebytes);

    return (void*)s_memblocks[idx].ptr;
}

void kfree_phy(void* ptr) {

    kmalloc_op_num++;

    //console_log(LOG_DEBUG, "kfree_phy: %u", kmalloc_op_num);

    uint64_t idx;
    for (idx = 0; idx <= s_last_memblock; idx++) {
        if (s_memblocks[idx].ptr == (uintptr_t)ptr) {
            break;
        }
    }

    ASSERT(idx <= s_last_memblock)

    s_memblocks[idx].flags &= ~(MEMBLOCK_FLAG_ALLOCATED);

    uint64_t idx_left = idx - 1;
    uint64_t idx_right = idx + 1;
    bool merged_blocks = false;
    uint64_t merge_idx = 0;

    // Check for an adjacent free block
    if (idx > 0) {
        if (!(s_memblocks[idx_left].flags & MEMBLOCK_FLAG_ALLOCATED)) {
            // Left is unallocated. Merge the two blocks
            s_memblocks[idx_left].size += s_memblocks[idx].size;
            merged_blocks = true;
            merge_idx = idx;
        }
    }

    if (idx_right <= s_last_memblock && !merged_blocks) {
        if (!(s_memblocks[idx_right].flags & MEMBLOCK_FLAG_ALLOCATED)) {
            // Right is unallocated. Merge the two blocks
            s_memblocks[idx].size += s_memblocks[idx_right].size;
            merged_blocks = true;
            merge_idx = idx_right;
        }
    }

    // If two free blocks were found, shift the reset of the blocks
    // to prevent holes in the structure
    if (merged_blocks) {
        ASSERT(merge_idx <= s_last_memblock);
        while ((merge_idx + 1) <= s_last_memblock) {
            s_memblocks[merge_idx] = s_memblocks[merge_idx+1];
            merge_idx++;
        }
        s_last_memblock--;
    }

    kmalloc_check_structure();

    // Ensure the memblock structure is consistent before continuing
    MEM_DMB();
}

void print_kmalloc_memblock(memblock_t* memblock) {
    //console_log(LOG_DEBUG, "Ptr: %16x", memblock->ptr);
    //console_log(LOG_DEBUG, "Size: %d", memblock->size);
    //console_log(LOG_DEBUG, "In Use: %c", (memblock->flags & MEMBLOCK_FLAG_ALLOCATED) ? 'Y' : 'N');
}

void print_kmalloc_debug(uint64_t alloc_size) {

    //console_log(LOG_DEBUG, "Alloc: %d", alloc_size);
}

void discover_phy_mem_dtb(mask_range_t* mask_ranges, uint64_t num_ranges) {

    // mask_ranges must be in order and non-overlapping

    discovery_dtb_ctx_t dtb_mem;
    int res = dtb_query_name("memory", &dtb_mem);

    int64_t added_size = 0;

    if (res == 0) {
        console_log(LOG_INFO, "Found memory node");
        ASSERT(dtb_mem.dt_node->prop_reg != NULL);

        for (int idx = 0; idx < dtb_mem.dt_node->prop_reg->num_regs; idx++) {
            dt_prop_reg_entry_t* reg_entry = &dtb_mem.dt_node->prop_reg->reg_entries[idx];
            ASSERT(reg_entry->addr_size == 2);
            ASSERT(reg_entry->size_size <= 2);

            uintptr_t reg_addr = ((uintptr_t)reg_entry->addr_ptr[0]) << 32 |
                                 ((uintptr_t)reg_entry->addr_ptr[1]);
            int64_t reg_size;
            if (reg_entry->size_size == 1) {
                reg_size = ((uintptr_t)reg_entry->size_ptr[0]);
            } else {
                reg_size = ((uintptr_t)reg_entry->size_ptr[0]) << 32 |
                           ((uintptr_t)reg_entry->size_ptr[1]);
            }
            uintptr_t reg_end = reg_addr + (uintptr_t)reg_size;

            for (int range_idx = 0; range_idx < num_ranges; range_idx++) {
                mask_range_t* range = &mask_ranges[range_idx];
                if (reg_addr >= range->start && reg_addr < range->end) {
                    // Range starts before reg and continues into the range. Ignore this section
                    reg_size -= (range->end - reg_addr);
                    reg_addr = range->end;
                    if (reg_size < 0) {
                        reg_size = 0;
                        break;
                    }
                } else if (range->start > reg_addr && range->start <= (reg_addr + reg_size)) {
                    // Range begins in the middle of reg
                    int64_t reg1_addr = reg_addr;
                    int64_t reg1_size = range->start - reg_addr;
                    if (reg1_size > 0x1000) {
                        kmalloc_add_phy_memory(reg1_addr, reg1_size);
                        added_size += reg1_size;
                    }

                    if (range->end < reg_end) {
                        reg_addr = range->end;
                        reg_size = reg_end - range->end;
                    }
                }
            }

            if (reg_size > 0x1000) {
                kmalloc_add_phy_memory(reg_addr, reg_size);
                added_size += reg_size;
            }
        }
    }
    ASSERT(res == 0);

    console_log(LOG_INFO, "Discovered memory: %d KB", added_size / 1024);
}
