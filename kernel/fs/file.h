
#ifndef __FS_FILE_H__
#define __FS_FILE_H__

#include <stdint.h>

#include <kernel/fd.h>
#include <kernel/lib/llist.h>
#include <kernel/lock/mutex.h>

typedef struct {
    uint8_t* data;
    uint64_t len;
    uint64_t offset;
    void* ctx;
    uint64_t dirty:1;
    uint64_t available:1;
} file_data_entry_t;

typedef void (*populate_data_fn)(void* ctx, file_data_entry_t* entry);
typedef void (*new_data_fn)(void* ctx, llist_head_t new_entries, uint64_t len);
typedef void (*flush_data_fn)(void* ctx, file_data_entry_t* entry);

typedef struct {
    llist_head_t data_list;
    int64_t size;

    uint64_t ref_count;
    lock_t ref_lock;

    fd_close_op close_op;
    populate_data_fn populate_op;
    new_data_fn new_data_op;
    flush_data_fn flush_data_op;
    void* op_ctx;
} file_data_t;

typedef struct {
    file_data_t* file_data;
    int64_t seek_idx;
    int64_t can_write:1;
    fd_ctx_t* fd_ctx;
} file_ctx_t;

void* file_create_ctx(file_ctx_t* file_ctx);

int64_t file_read_op(void* ctx, uint8_t* buffer, const int64_t size, const uint64_t flags);
int64_t file_write_op(void* ctx, const uint8_t* buffer, const int64_t size, const uint64_t flags);
int64_t file_ioctl_op(void* ctx, const uint64_t ioctl, const uint64_t* args, const uint64_t arg_count);
int64_t file_close_op(void* ctx);

#endif