
#include <stdint.h>
#include <stdbool.h>


#ifdef KERNEL_BUILD
#include "kernel/assert.h"
#define SYS_ASSERT(x) ASSERT(x)
#else
#include "system/lib/system_assert.h"
#endif


#include "stdlib/malloc.h"

#include "stdlib/bitutils.h"

#include "include/k_syscall.h"

#define MIN_LEFTOVER_SIZE 64
#define MALLOC_MAGIC (0xA5A51F1FBCBC9393ULL)

struct _malloc_entry_t;

malloc_state_t s_malloc_state;

/**
 * Memory layout
 * 
 * malloc_state_t contains pointers to the memory region
 * owned by malloc. The malloc state also contains a pointer
 * to the first malloc_entry_t. malloc_entry_t structs contain
 * metadata about the proceeding chunk. It is assumed that this
 * chunk likes immediately after the entry in memory. The final
 * chunk is designated with a NULL next pointer
 */

void malloc_check_structure_p(malloc_state_t* state) {
    return;

    SYS_ASSERT(state->magic == MALLOC_MAGIC);

    malloc_entry_t* curr_entry = state->first_entry;
    do {
        SYS_ASSERT(curr_entry->magic == MALLOC_MAGIC);
        if (curr_entry->next != NULL) {
            SYS_ASSERT(curr_entry->next == (curr_entry->chunk_start + curr_entry->size));
        } else {
            SYS_ASSERT(state->limit == (uintptr_t)(curr_entry->chunk_start + curr_entry->size));
        }

        curr_entry = curr_entry->next;
    } while(curr_entry != NULL);
}

void malloc_init_p(malloc_state_t* state, malloc_add_mem_func add_mem_func, void* ctx) {
    SYS_ASSERT(state != NULL);

    int64_t ret = add_mem_func(true, 0, ctx, state);
    SYS_ASSERT(ret > 0);
    // if (add_mem_func != NULL) {
    //     // SYSCALL_CALL_RET(SYSCALL_SBRK, 0, 0, 0, 0, base);
    //     // SYS_ASSERT(base > 0);

    //     // SYSCALL_CALL_RET(SYSCALL_SBRK, MIN_MALLOC_SBRK_SIZE, 0, 0, 0, limit);
    //     // SYS_ASSERT(limit > 0);
    //     SYS_ASSERT(0);
    // } else {
    //     base = (uint64_t)mem;
    //     limit = base + len;
    // }
    // state->base = base;
    // state->limit = limit;

    malloc_entry_t* first_entry = (malloc_entry_t*)state->base;
    first_entry->magic = MALLOC_MAGIC;
    first_entry->next = NULL;
    first_entry->size = (state->limit - state->base) - sizeof(malloc_entry_t);
    first_entry->inuse = false;
    first_entry->chunk_start = first_entry + 1;

    state->first_entry = first_entry;
    state->add_mem_func = add_mem_func;
    state->num_malloc_ops = 0;
    state->magic = MALLOC_MAGIC;
}


void* malloc_p(uint64_t size, malloc_state_t* state) {
    SYS_ASSERT(state != NULL);
    SYS_ASSERT(state->magic == MALLOC_MAGIC);
    state->num_malloc_ops++;

    uint64_t size_align = (size + sizeof(uint64_t) - 1) & (~(sizeof(uint64_t) - 1));

    malloc_check_structure_p(state);
    malloc_entry_t* curr_entry = state->first_entry;

    do {
        // Basic protection from buffer overflow
        SYS_ASSERT(curr_entry->magic == MALLOC_MAGIC);

        if ((!curr_entry->inuse) &&
            (curr_entry->size >= size_align)) {
            break;
        }

        curr_entry = curr_entry->next;
    } while (curr_entry != NULL);

    if (curr_entry == NULL) {
        // Add memory
        int64_t increase;
        increase = state->add_mem_func(false, size_align, state->add_mem_ctx, state);
        SYS_ASSERT(increase > 0);

        malloc_entry_t* last_entry = state->first_entry;
        while (last_entry->next != NULL) {
            last_entry = last_entry->next;
        }

        if (last_entry->inuse) {
            malloc_entry_t* new_entry = (malloc_entry_t*)(last_entry->chunk_start + last_entry->size);
            new_entry->magic = MALLOC_MAGIC;
            new_entry->next = NULL;
            new_entry->size = increase - sizeof(malloc_entry_t);
            new_entry->inuse = false;
            new_entry->chunk_start = new_entry + 1;
            last_entry->next = new_entry;
        } else {
            last_entry->size += increase;
        }

        // Re-run malloc. This should return valid memory
        return malloc_p(size_align, state);
    } else {
        uint64_t leftover = curr_entry->size - size_align;
        curr_entry->inuse = true;

        if (leftover < MIN_LEFTOVER_SIZE) {
            // Don't fragment the memory space too much. If we have less than
            // MIN_LEFTOVER_SIZE bytes left in the entry, just use the entire entry
            malloc_check_structure_p(state);
            return curr_entry->chunk_start;
        } else {
            // Need to split the chunk into two
            void* chunk_start = curr_entry->chunk_start;

            malloc_entry_t* new_entry = chunk_start + size_align;

            new_entry->magic = MALLOC_MAGIC;
            new_entry->next = curr_entry->next;
            new_entry->size = leftover - sizeof(malloc_entry_t);
            new_entry->inuse = false;
            new_entry->chunk_start = new_entry + 1;

            curr_entry->size = size_align;
            curr_entry->next = new_entry;
        }

        malloc_check_structure_p(state);
        return curr_entry->chunk_start;
    }
}

void free_p(const void* mem, malloc_state_t* state) {
    SYS_ASSERT(mem != NULL);
    SYS_ASSERT(state->magic == MALLOC_MAGIC);
    state->num_malloc_ops++;

    malloc_entry_t* entry = ((malloc_entry_t*)mem) - 1;

    if (entry->magic == MALLOC_MAGIC &&
        entry->inuse == true &&
        entry->chunk_start == mem) {

        // Entry seems valid. Free the entry
        entry->inuse = false;
    
        if (entry->next != NULL &&
            entry->next->inuse == false) {

            entry->size = entry->size + entry->next->size + sizeof(malloc_entry_t);
            entry->next = entry->next->next;
       }
    } else {
        // Something is wrong with the entry. Give up
        SYS_ASSERT(false);
    }

    malloc_check_structure_p(state);
}

void malloc_calc_stat(malloc_state_t* state, malloc_stat_t* stat_out) {

    uint64_t total_mem = state->limit - state->base;

    uint64_t avail_mem = 0;
    uint64_t largest_chunk = 0;

    malloc_entry_t* entry = state->first_entry;
    while (entry != NULL) {

        if (!entry->inuse) {
            avail_mem += entry->size;

            if (entry->size > largest_chunk) {
                largest_chunk = entry->size;
            }
        }

        entry = entry->next;
    }


    stat_out->total_mem = total_mem;
    stat_out->avail_mem = avail_mem;
    stat_out->largest_chunk = largest_chunk;
}

// void malloc_init(void) {
//     malloc_init_p(&s_malloc_state, malloc_add_mem_p, NULL, 0);
// }

// void* malloc(uint64_t size) {
//     return malloc_p(size, &s_malloc_state);
// }

// void free(void* mem) {
//     free_p(mem, &s_malloc_state);
// }