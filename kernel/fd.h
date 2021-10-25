
#ifndef __FD_H__
#define __FD_H__

#include <stdint.h>

typedef int64_t (*fd_read_op)(void* ctx, uint8_t* buffer, const int64_t size, const uint64_t flags);
typedef int64_t (*fd_write_op)(void* ctx, const uint8_t* buffer, const int64_t size, const uint64_t flags);
typedef int64_t (*fd_seek_op)(void* ctx, const int64_t pos, const uint64_t flags);
typedef int64_t (*fd_ioctl_op)(void* ctx, const uint64_t ioctl, const uint64_t arg1, const uint64_t arg2, const void* addition, const uint64_t size);
typedef int64_t (*fd_close_op)(void* ctx);

typedef struct {
    fd_read_op read;
    fd_write_op write;
    fd_seek_op seek;
    fd_ioctl_op ioctl;
    fd_close_op close;
} fd_ops_t;

typedef struct {
    bool valid;
    void* ctx;
    fd_ops_t ops;
} fd_ctx_t;

#endif