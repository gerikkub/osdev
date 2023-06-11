
#include <stdint.h>

#include "kernel/lib/hashmap.h"
#include "kernel/lib/vmalloc.h"

#include "stdlib/bitutils.h"

uint64_t uintmap_hash_op(void* key) {
    return *(uint64_t*)key;
}

bool uintmap_cmp_op(void* key1, void* key2) {
    return *(uint64_t*)key1 == *(uint64_t*)key2;
}

void uintmap_free_op(void* ctx, void* key, void* dataptr) {
    vfree(key);
    vfree(dataptr);
}


hashmap_ctx_t* uintmap_alloc(uint64_t init_log_len) {
    return hashmap_alloc(uintmap_hash_op,
                         uintmap_cmp_op,
                         uintmap_free_op,
                         init_log_len,
                         NULL);
}