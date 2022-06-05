
#ifndef __FS_FILE_H__
#define __FS_FILE_H__

#include <stdint.h>

#include <kernel/fd.h>
#include <kernel/lib/llist.h>


typedef struct {
    uint8_t* data;
    uint64_t len;
    bool dirty;
} file_data_entry_t;

typedef struct {
    llist_head_t file_data;
    int64_t size;
    int64_t seek_idx;
    bool can_write;
    fd_close_op close_op;
    void* free_ctx;
} file_ctx_t;

void* file_create_ctx(file_ctx_t* file_ctx);

int64_t file_read_op(void* ctx, uint8_t* buffer, const int64_t size, const uint64_t flags);
int64_t file_write_op(void* ctx, const uint8_t* buffer, const int64_t size, const uint64_t flags);
int64_t file_ioctl_op(void* ctx, const uint64_t ioctl, const uint64_t* args, const uint64_t arg_count);
int64_t file_close_op(void* ctx);

#endif