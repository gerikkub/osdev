
#include <stdint.h>
#include <stdlib.h>

#include "kernel/lib/vmalloc.h"
#include "kernel/assert.h"
#include "kernel/lib/llist.h"

#include "stdlib/string.h"

#include "kernel/lib/hashmap.h"

typedef struct {
    void* key;
    void* dataptr;
} hashmap_list_entry_t;

hashmap_ctx_t* hashmap_alloc(hashmap_hash_fn hash_op,
                             hashmap_cmp_fn cmp_op,
                             hashmap_free_fn free_op,
                             uint64_t init_log_len,
                             void* op_ctx) {

    hashmap_ctx_t* hashmap_ctx = vmalloc(sizeof(hashmap_ctx_t));

    hashmap_ctx->hashtable = vmalloc(sizeof(llist_head_t) * (1 << init_log_len));
    memset(hashmap_ctx->hashtable, 0, sizeof(llist_head_t) * (1 << init_log_len));

    hashmap_ctx->hashtable_log_len = init_log_len;
    hashmap_ctx->entries = 0;
    hashmap_ctx->hash_op = hash_op;
    hashmap_ctx->cmp_op = cmp_op;
    hashmap_ctx->free_op = free_op;
    hashmap_ctx->op_ctx = op_ctx;

    return hashmap_ctx;
}

void hashmap_dealloc(hashmap_ctx_t* ctx) {

    int idx;
    for (idx = 0; idx < (1 << ctx->hashtable_log_len); idx++) {
        llist_head_t head = ctx->hashtable[idx];

        if (head != NULL) {
            hashmap_list_entry_t* entry;
            FOR_LLIST(head, entry)
                if (ctx->free_op) {
                    ctx->free_op(ctx->op_ctx, entry->key, entry->dataptr);
                }
                vfree(entry);
            END_FOR_LLIST()
            llist_free_all(head);
        }
    }

    vfree(ctx->hashtable);
    vfree(ctx);
}

void* hashmap_get(hashmap_ctx_t* ctx, void* key) {

    uint64_t keyhash = ctx->hash_op(key) & ((1 << ctx->hashtable_log_len) - 1);

    llist_head_t keylist = ctx->hashtable[keyhash];

    if (keylist == NULL) {
        return NULL;
    }

    hashmap_list_entry_t* entry;
    FOR_LLIST(keylist, entry)
        if (ctx->cmp_op(key, entry->key)) {
            return entry->dataptr;
        }
    END_FOR_LLIST()

    return NULL;
}

bool hashmap_contains(hashmap_ctx_t* ctx, void* key) {

    uint64_t keyhash = ctx->hash_op(key) & ((1 << ctx->hashtable_log_len) - 1);

    llist_head_t keylist = ctx->hashtable[keyhash];

    if (keylist == NULL) {
        return false;
    }

    hashmap_list_entry_t* entry;
    FOR_LLIST(keylist, entry)
        if (ctx->cmp_op(key, entry->key)) {
            return true;
        }
    END_FOR_LLIST()

    return false;
}

void* hashmap_del(hashmap_ctx_t* ctx, void* key) {

    void* entry_key;
    uint64_t keyhash = ctx->hash_op(key) & ((1 << ctx->hashtable_log_len) - 1);

    llist_head_t keylist = ctx->hashtable[keyhash];

    if (keylist == NULL) {
        return NULL;
    }

    hashmap_list_entry_t* entry;
    hashmap_list_entry_t* delentry = NULL;
    FOR_LLIST(keylist, entry)
        if (ctx->cmp_op(key, entry->key)) {
            entry_key = entry->key;
            delentry = entry;
        }
    END_FOR_LLIST()
    
    if (delentry != NULL) {
        if (ctx->free_op) {
            ctx->free_op(ctx->op_ctx, entry->key, entry->dataptr);
        }
        llist_delete_ptr(keylist, delentry);

        if (llist_empty(keylist)) {
            llist_free_all(keylist);
            ctx->hashtable[keyhash] = NULL;
        }

        ctx->entries--;

        return entry_key;
    }

    return NULL;
}

void hashmap_add(hashmap_ctx_t* ctx, void* key, void* dataptr) {

    uint64_t keyhash = ctx->hash_op(key) & ((1 << ctx->hashtable_log_len) - 1);

    llist_head_t keylist = ctx->hashtable[keyhash];

    if (keylist == NULL) {
        keylist = llist_create();
        ctx->hashtable[keyhash] = keylist;
    }

    hashmap_list_entry_t* entry = vmalloc(sizeof(hashmap_list_entry_t));
    entry->key = key;
    entry->dataptr = dataptr;

    llist_append_ptr(keylist, entry);
    ctx->entries++;
}

uint64_t hashmap_len(hashmap_ctx_t* ctx) {
    return ctx->entries;
}

uint64_t hashmap_tablelen(hashmap_ctx_t* ctx) {
    return (1 << ctx->hashtable_log_len);
}

void hashmap_rehash(hashmap_ctx_t* ctx, uint64_t newlen) {

}
