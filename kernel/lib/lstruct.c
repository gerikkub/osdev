
#include <stdint.h>

#include "bitutils.h"

#include "lstruct.h"

void lstruct_init_head(lstruct_head_t* head) {
    (*head)->n = NULL;
    (*head)->p = NULL;
}

void lstruct_append(lstruct_head_t* head, lstruct_t* newentry) {
    while ((*head)->n) {
        head = &(*head)->n;
    }

    newentry->n = NULL;
    newentry->p = *head;
    MEM_DMB();
    (*head)->n = newentry;
}

void lstruct_prepend(lstruct_head_t* head, lstruct_t* newentry) {
    newentry->n = (*head)->n;
    newentry->p = *head;
    newentry->n->p = newentry;
    MEM_DMB();
    (*head)->n = newentry;
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