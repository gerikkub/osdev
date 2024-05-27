
#ifndef __LSTRUCT_H__
#define __LSTRUCT_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

struct lstruct_;

typedef struct lstruct_ {
    struct lstruct_* n;
    struct lstruct_* p;
} lstruct_t;

typedef lstruct_t* lstruct_head_t;

void lstruct_init_head(lstruct_head_t* head);
void lstruct_append(lstruct_head_t head, lstruct_t* newentry);
void lstruct_prepend(lstruct_head_t head, lstruct_t* newentry);
void lstruct_insert_after(lstruct_t* entry, lstruct_t* newentry);
void lstruct_remove(lstruct_t* entry);
bool lstruct_empty(lstruct_head_t head);
int64_t lstruct_len(lstruct_head_t head);

lstruct_t* lstruct_list_at(lstruct_head_t head, int idx);

#define CONCAT_(a, b) a ## b
#define CONCAT(a, b) CONCAT_(a, b)

#define FOREACH_LSTRUCT(head, ptr, name) \
    lstruct_t* CONCAT(walker, __LINE__) = head->n; \
    while ( CONCAT(walker, __LINE__) == NULL ? 0 : \
                ((ptr = (void*)((uintptr_t)((CONCAT(walker, __LINE__))) - offsetof(typeof(*ptr), name))), \
                 (CONCAT(walker, __LINE__) = CONCAT(walker, __LINE__)->n ), \
                 1))

#define LSTRUCT_AT(head, idx, type, name) \
    ((type*)(((uintptr_t)lstruct_list_at(head, idx) ? : (offsetof(type, name))) - offsetof(type, name)))
    


#endif