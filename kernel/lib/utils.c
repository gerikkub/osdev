
#include "kernel/lib/vmalloc.h"

#include "stdlib/bitutils.h"
#include <string.h>

char** path_parser(const char* path, const char delim) {

    if (path[0] == '/') {
        path++;
    }

    int delim_count = 0;
    for (int idx = 0; path[idx] != 0; idx++) {
        if (path[idx] == delim) {
            delim_count++;
        }
    }

    char** path_list = vmalloc((delim_count + 2) * sizeof(char*));
    memset(path_list, 0, (delim_count + 2) * sizeof(char*));

    int path_idx = 0;
    int start_idx = 0;
    while (path[start_idx] != '\0') {
        int end_idx = start_idx;
        while (path[end_idx] != '\0' &&
               path[end_idx] != delim) {
            end_idx++;
        }

        const int seg_len = end_idx - start_idx;
        path_list[path_idx] = vmalloc(seg_len + 1);
        memcpy(path_list[path_idx], &path[start_idx], seg_len);
        path_list[path_idx][seg_len] = '\0';

        if (path[end_idx] == '\0') {
            break;
        }
        start_idx = end_idx + 1;
    }

    return path_list;
}

void free_path_list(char** path_list) {
    int idx = 0;
    while (path_list[idx] != NULL) {
        vfree(path_list[idx]);
        idx++;
    }
    vfree(path_list);
}

