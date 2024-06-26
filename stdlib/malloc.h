
#ifndef __SYSTEM_MALLOC_H__
#define __SYSTEM_MALLOC_H__

#include <stdint.h>
#include <stdbool.h>

typedef struct _malloc_entry_t {
    uint64_t magic;                 /* Equal to MALLOC_MAGIC */
    struct _malloc_entry_t* next;   /* Pointer to the next malloc_entry, or NULL if the last entry */
    uint64_t size;                  /* Size of the associated chunk */
    bool inuse;                     /* Is the chunk currently allocated? */
    void* chunk_start;              /* Pointer to the beginnning of chunk memory */
} malloc_entry_t;

struct _malloc_state_t;

typedef int64_t (*malloc_add_mem_func)(bool initialize, uint64_t req_size, void* ctx, struct _malloc_state_t* state);

typedef struct _malloc_state_t{
    uintptr_t base;                 /* Base address of malloc's memory */
    uintptr_t limit;                /* Limit address of malloc's memory */
    malloc_entry_t* first_entry;    /* First entry. Should be the same as the base pointer */
    uint64_t magic;                 /* Equal to MALLOC_MAGIC */
    malloc_entry_t* last_checked_entry;
    malloc_add_mem_func add_mem_func;
    uint64_t num_malloc_ops;
    void* add_mem_ctx;
} malloc_state_t;

typedef struct {
    uint64_t total_mem;
    uint64_t avail_mem;
    uint64_t largest_chunk;
} malloc_stat_t;

void malloc_init_p(malloc_state_t* state, malloc_add_mem_func add_mem_func, void* ctx);
void* malloc_p(uint64_t size, malloc_state_t* state);
void free_p(const void* mem, malloc_state_t* state);
void malloc_check_structure_p(malloc_state_t* state);

void malloc_calc_stat(malloc_state_t* state, malloc_stat_t* stat_out);

#endif