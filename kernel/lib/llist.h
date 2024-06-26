
#ifndef __LLIST_H__
#define __LLIST_H__

#include <stdint.h>
#include <stdbool.h>

#include "kernel/assert.h"

struct llist_struct;

typedef struct llist_struct {
    struct llist_struct* n;
    struct llist_struct* p;
    void* dataptr;
} llist_t;

typedef llist_t* llist_head_t;

llist_head_t llist_create();

void llist_free(llist_head_t);
void llist_free_all(llist_head_t);

// TODO: Break doesn't work inside a
//       FOR_LLIST
#define FOR_LLIST(head, x) \
{ \
x = NULL; \
llist_t* __for_llist_item = head->n; \
while (__for_llist_item != NULL) {\
ASSERT((uintptr_t)__for_llist_item > 64); \
x = __for_llist_item->dataptr; \
__for_llist_item = __for_llist_item->n; \
do {

#define END_FOR_LLIST() } while(0); } } \

#define LLIST_FIND(head, x) llist_find_helper(head, x, sizeof(x))

llist_t* llist_find_helper(llist_head_t head, void* x, uint64_t len);

void llist_prepend_ptr(llist_head_t head, void* newitem);

void llist_append_ptr(llist_head_t head, void* newitem);

void llist_delete_ptr(llist_head_t head, void* delitem);

bool llist_empty(llist_head_t head);

int64_t llist_len(llist_head_t head);

void* llist_at(llist_head_t head, int64_t idx);

#endif
