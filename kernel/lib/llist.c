
#include <stdint.h>
#include <string.h>

#include "kernel/lib/vmalloc.h"
#include "kernel/kmalloc.h"
#include "kernel/kernelspace.h"
#include "kernel/assert.h"
#include "kernel/lib/vmalloc.h"

#include "kernel/lib/llist.h"

// llist structure
//

// llist_head_t -> 
//
// /------------------\   /------------------\  /------------------|
// | n ------------------>| n ----------------->| n -> NULL        |
// | NULL <- p        |<--- p                |<--- p               |
// | dataptr -> NULL  |   | dataptr -> item1 |  | dataptr -> item2 |
// \------------------/   \------------------/  \------------------/



llist_head_t llist_create() {
    llist_t* l = vmalloc(sizeof(llist_t));
    l->n = NULL;
    l->p = NULL;
    l->dataptr = NULL;

    return l;
}

void llist_free(llist_head_t head) {
    vfree(head);
}

void llist_free_all(llist_head_t head) {

    llist_t* item = head->n;
    while (item != NULL) {
        llist_t* n = item->n;
        vfree(item);
        item = n;
    }

    head->n = NULL;
    head->p = NULL;
    head->dataptr = NULL;
}


llist_t* llist_find_helper(llist_head_t head, void* x, uint64_t len) {

    llist_t* item = head->n;

    while (item != NULL) {
        if (memcmp(x, item->dataptr, len) == 0) {
            return item;
        }
        item = item->n;
    }

    return NULL;
}

void llist_append_ptr(llist_head_t head, void* newitem) {

    llist_t* item = head;

    while (item->n != NULL) {
        item = item->n;
    }

    item->n = vmalloc(sizeof(llist_t));
    item->n->dataptr = newitem;
    item->n->n = NULL;
    item->n->p = item;
}

void llist_delete_ptr(llist_head_t head, void* delitem) {

    llist_t* item = head;

    if (item->n == NULL) {
        return;
    }

    while (item->n != NULL && item->dataptr != delitem) {
        item = item->n;
    }

    if (item->dataptr == delitem) {
        if (item->n != NULL) {
            item->n->p = item->p;
        }
        item->p->n = item->n;

        item->dataptr = NULL;
        vfree(item);
    }
}

bool llist_empty(llist_head_t head) {
    return llist_len(head) == 0;
}

int64_t llist_len(llist_head_t head) {
    int64_t len = 0;
    void* item = NULL;
    FOR_LLIST(head, item)
        len++;
        (void)item;
    END_FOR_LLIST()
    return len;
}

void* llist_at(llist_head_t head, int64_t idx) {
    int64_t curr_idx = 0;
    void* item;
    FOR_LLIST(head, item)
        if (idx == curr_idx) {
            return item;
        }
        curr_idx++;
    END_FOR_LLIST()

    return NULL;
}