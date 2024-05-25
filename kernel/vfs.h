
#ifndef __VFS_H__
#define __VFS_H__

#include <stdint.h>

#include "kernel/fd.h"

#define MAX_DEVICE_NAME_LEN 32

typedef int64_t (*device_open_op)(void* ctx, const char* path, const uint64_t flags, void** ctx_out, fd_ctx_t* fd_ctx);

typedef struct {
    bool valid;
    device_open_op open;
    void* ctx;
    fd_ops_t fd_ops;
    char name[MAX_DEVICE_NAME_LEN];
} vfs_device_ops_t;

void vfs_register_device(vfs_device_ops_t* device);

int64_t vfs_open_device(const char* device_name,
                        const char* path,
                        uint64_t flags,
                        fd_ops_t* ops_out,
                        void** ctx_out,
                        fd_ctx_t* fd_ctx);

int64_t vfs_open_device_fd(const char* device_name,
                           const char* path,
                           uint64_t flags);

#endif