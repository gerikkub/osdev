
#include <stdint.h>

#include "kernel/lib/vmalloc.h"
#include "kernel/kmalloc.h"
#include "kernel/kernelspace.h"
#include "kernel/assert.h"
#include "kernel/lib/vmalloc.h"

#include "stdlib/string.h"

#include "kernel/lib/llist.h"

llist_head_t llist_create() {
    llist_t* l = vmalloc(sizeof(llist_t));
    l->n = NULL;
    l->p = NULL;
    l->dataptr = NULL;

    return l;
}

llist_t* llist_find_helper(llist_head_t head, void* x, uint64_t len) {

    llist_t* item = head;

    while (item->n != NULL) {
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
    item->dataptr = newitem;
    item->n->n = NULL;
    item->n->p = item;
}

void llist_delete_ptr(llist_head_t head, void* delitem) {

    llist_t* item = head;

    while (item->n != NULL && item->dataptr != delitem) {
    }

    if (item->dataptr == delitem) {
        item->n->p = item->p;
        if (item->p != NULL) {
            item->p->n = item->n;
        }

        vfree(item);
    }
}

