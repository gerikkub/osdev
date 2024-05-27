
#include <stdint.h>

#include "kernel/lib/vmalloc.h"

#include "bitutils.h"

#include "lstruct.h"


void lstruct_init_head(lstruct_head_t* head) {
    *head = vmalloc(sizeof(lstruct_t));
    (*head)->n = NULL;
    (*head)->p = NULL;
}

void lstruct_append(lstruct_head_t head, lstruct_t* newentry) {
    while (head->n) {
        head = head->n;
    }

    newentry->n = NULL;
    newentry->p = head;
    MEM_DMB();
    head->n = newentry;
}

void lstruct_prepend(lstruct_head_t head, lstruct_t* newentry) {
    newentry->n = head->n;
    newentry->p = head;
    if (newentry->n) {
        newentry->n->p = newentry;
    }
    MEM_DMB();
    head->n = newentry;
}

void lstruct_insert_after(lstruct_t* entry, lstruct_t* newentry) {
    newentry->p = entry;
    newentry->n = entry->n;
    if (newentry->n) newentry->n->p = newentry;
    MEM_DMB();
    entry->n = newentry;
}

void lstruct_remove(lstruct_t* entry) {
    entry->p->n = entry->n;
    if (entry->n) {
        entry->n->p = entry->p;
    }
    MEM_DMB();
}

bool lstruct_empty(lstruct_head_t head) {
    return head->n == NULL;
}

int64_t lstruct_len(lstruct_head_t head) {
    int64_t count = 0;
    head = head->n;
    while(head != NULL) {
        head = head->n;
        count++;
    }
    return count;
}

lstruct_t* lstruct_list_at(lstruct_head_t head, int idx) {
    head = head->n;
    while(head != NULL && idx--) {
        head = head->n;
    }

    return head;
}