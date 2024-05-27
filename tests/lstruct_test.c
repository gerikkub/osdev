
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "test_helpers.h"

#include "kernel/lib/lstruct.h"

typedef struct {
    int a;
    lstruct_t list;
} test_type_t;

void print_list(lstruct_head_t head) {

    test_type_t* entry;
    FOREACH_LSTRUCT(head, entry, list) {
        printf("%d ", entry->a);
    }
    printf("\n");
}

int main(int argc, char** argv) {

    const int N = 10;

    test_type_t* items = malloc(sizeof(test_type_t) * N);
    lstruct_head_t head, head2;
    lstruct_init_head(&head);
    lstruct_init_head(&head2);

    assert(lstruct_empty(head));
    assert(lstruct_empty(head2));
    assert(lstruct_len(head) == 0);

    for (int idx = 0; idx < N; idx++) {
        items[idx].a = idx;
        lstruct_append(head, &items[idx].list);
    }
    const int check1[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    int check1_idx = 0;

    check1_idx = 0;
    test_type_t* entry;
    FOREACH_LSTRUCT(head, entry, list) {
        assert(entry != NULL);
        assert(entry == &items[check1_idx]);
        assert(entry->a == check1[check1_idx]);
        check1_idx++;
    }
    assert(check1_idx == 10);

    assert(LSTRUCT_AT(head, 0, test_type_t, list)->a == 0);
    assert(LSTRUCT_AT(head, 4, test_type_t, list)->a == 4);
    assert(LSTRUCT_AT(head, 10, test_type_t, list) == NULL);

    assert(!lstruct_empty(head));
    assert(lstruct_len(head) == 10);

    for (int idx = 0; idx < N; idx++) {
        if (idx % 2 == 0) {
            lstruct_remove(&items[idx].list);
        }
    }

    const int check2[] = {1, 3, 5, 7, 9};
    int check2_idx = 0;

    print_list(head);
    FOREACH_LSTRUCT(head, entry, list) {
        assert(entry != NULL);
        assert(entry->a == check2[check2_idx]);
        check2_idx++;
    }
    assert(check2_idx == 5);

    for (int idx = 0; idx < N; idx++) {
        if (idx % 2 == 0) {
            lstruct_prepend(head, &items[idx].list);
        }
    }

    const int check3[] = {8, 6, 4, 2, 0, 1, 3, 5, 7, 9};
    int check3_idx = 0;

    print_list(head);
    FOREACH_LSTRUCT(head, entry, list) {
        assert(entry != NULL);
        assert(entry->a == check3[check3_idx]);
        check3_idx++;
    }
    assert(check3_idx == 10);

    lstruct_remove(&items[9].list);
    lstruct_insert_after(&items[4].list, &items[9].list);

    const int check4[] = {8, 6, 4, 9, 2, 0, 1, 3, 5, 7};
    int check4_idx = 0;

    print_list(head);
    FOREACH_LSTRUCT(head, entry, list) {
        assert(entry != NULL);
        assert(entry->a == check4[check4_idx]);
        check4_idx++;
    }
    assert(check4_idx == 10);


    int idx = 0;
    FOREACH_LSTRUCT(head, entry, list) {
        assert(entry != NULL);
        if (entry->a % 3 != 0) {
            lstruct_remove(&entry->list);
            lstruct_append(head2, &entry->list);
        }
        idx++;
    }

    const int check5[] = {6, 9, 0, 3};
    int check5_idx = 0;

    print_list(head);
    FOREACH_LSTRUCT(head, entry, list) {
        assert(entry != NULL);
        assert(entry->a == check5[check5_idx]);
        check5_idx++;
    }
    assert(check5_idx == 4);
    assert(lstruct_len(head) == 4);

    const int check6[] = {8, 4, 2, 1, 5, 7};
    int check6_idx = 0;


    print_list(head2);
    FOREACH_LSTRUCT(head2, entry, list) {
        assert(entry != NULL);
        assert(entry->a == check6[check6_idx]);
        check6_idx++;
    }
    assert(check6_idx == 6);

    return 0;
}
