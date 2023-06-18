
#ifndef __HASHMAP_H__
#define __HASHMAP_H__

#include <stdint.h>

#include "kernel/lib/llist.h"


typedef uint64_t (*hashmap_hash_fn)(void* key);
typedef bool (*hashmap_cmp_fn)(void* key1, void* key2);
typedef void (*hashmap_free_fn)(void* ctx, void* key, void* dataptr);

typedef void (*hashmap_forall_fn)(void* ctx, void* check_ctx, void* key, void* dataptr);

typedef struct {
    llist_head_t* hashtable;
    uint64_t hashtable_log_len;
    uint64_t entries;
    hashmap_hash_fn hash_op;
    hashmap_cmp_fn cmp_op;
    hashmap_free_fn free_op;
    void* op_ctx;
} hashmap_ctx_t;


hashmap_ctx_t* hashmap_alloc(hashmap_hash_fn hash_op,
                             hashmap_cmp_fn cmp_op,
                             hashmap_free_fn free_op,
                             uint64_t init_log_len,
                             void* op_ctx);

void hashmap_dealloc(hashmap_ctx_t* ctx);

void* hashmap_get(hashmap_ctx_t* ctx, void* key);
bool hashmap_contains(hashmap_ctx_t* ctx, void* key);
void hashmap_add(hashmap_ctx_t* ctx, void* key, void* dataptr);
void* hashmap_del(hashmap_ctx_t* ctx, void* key);

uint64_t hashmap_len(hashmap_ctx_t* ctx);
uint64_t hashmap_tablelen(hashmap_ctx_t* ctx);

void hashmap_rehash(hashmap_ctx_t* ctx, uint64_t newlen);

void hashmap_forall(hashmap_ctx_t* ctx, hashmap_forall_fn fn, void* forall_ctx);


#endif