
#ifndef __LLIST_H__
#define __LLIST_H__

struct llist_struct;

typedef struct llist_struct {
    struct llist_struct* n;
    struct llist_struct* p;
    void* dataptr;
} llist_t;

typedef llist_t* llist_head_t;

llist_head_t llist_create();

#define LLIST_FIND(head, x) llist_find_heaper(head, x, sizeof(x))

llist_t* llist_find_helper(llist_head_t head, void* x, uint64_t len);

void llist_append_ptr(llist_head_t head, void* newitem);

void llist_delete_ptr(llist_head_t head, void* delitem);

#endif
