
#ifndef __SYSTEM_MALLOC_H__
#define __SYSTEM_MALLOC_H__

#include <stdlib.h>

void malloc_init();

void* malloc(uint64_t size);

void free(void* mem);

typedef struct _malloc_entry_t {
    uint64_t magic;                 /* Equal to MALLOC_MAGIC */
    struct _malloc_entry_t* next;   /* Pointer to the next malloc_entry, or NULL if the last entry */
    uint64_t size;                  /* Size of the associated chunk */
    bool inuse;                     /* Is the chunk currently allocated? */
    void* chunk_start;              /* Pointer to the beginnning of chunk memory */
} malloc_entry_t;

struct _malloc_state_t;

typedef bool (*malloc_add_mem_func)(uint64_t req_size, struct _malloc_state_t* state);

typedef struct _malloc_state_t{
    uintptr_t base;                 /* Base address of malloc's memory */
    uintptr_t limit;                /* Limit address of malloc's memory */
    malloc_entry_t* first_entry;    /* First entry. Should be the same as the base pointer */
    uint64_t magic;                 /* Equal to MALLOC_MAGIC */
    malloc_add_mem_func add_mem_func;
    uint64_t num_malloc_ops;
} malloc_state_t;

void malloc_init_p(malloc_state_t* state, malloc_add_mem_func add_mem_func, void* mem, uint64_t len);
void* malloc_p(uint64_t size, malloc_state_t* state);
void free_p(void* mem, malloc_state_t* state);

#endif