
#ifndef __LLIST_H__
#define __LLIST_H__

#include <stdint.h>
#include <stddef.h>

struct lstruct_;

typedef struct lstruct_ {
    struct lstruct_* n;
    struct lstruct_* p;
} lstruct_t;

typedef lstruct_t* lstruct_head_t;

void lstruct_init_head(lstruct_head_t* head);
void lstruct_append(lstruct_head_t* head, lstruct_t* newentry);
void lstruct_prepend(lstruct_head_t* head, lstruct_t* newentry);
void lstruct_insert_after(lstruct_t* entry, lstruct_t* newentry);
void lstruct_remove(lstruct_t* entry);

#define CONCAT_(a, b) a ## b
#define CONCAT(a, b) CONCAT_(a, b)

#define FOREACH_LSTRUCT(head, ptr, name) \
    lstruct_t* CONCAT(walker, __LINE__) = head->n; \
    while ( CONCAT(walker, __LINE__) == NULL ? 0 : \
                ((ptr = (void*)((uintptr_t)((CONCAT(walker, __LINE__))) - offsetof(typeof(*ptr), list))), \
                 (CONCAT(walker, __LINE__) = CONCAT(walker, __LINE__)->n ), \
                 1))

    //while ( ptr = (void*)(((uintptr_t)(((lstruct_t*) (((uintptr_t)ptr) + offsetof(typeof(*ptr), list)))->n)) - offsetof(typeof(*ptr), list)), ((uintptr_t)ptr) + offsetof(typeof(*ptr), list))

//#define FOREACH_LSTRUCT_INT2(head, ptr, name, tag) FOREACH_LSTRUCT_INT(head, ptr, name, tag)
//#define FOREACH_LSTRUCT(head, ptr, name) FOREACH_LSTRUCT_INT(head, ptr, name, __LINE__)

#endif