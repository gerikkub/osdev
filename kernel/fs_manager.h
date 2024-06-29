
#ifndef __FS_MANAGER_H__
#define __FS_MANAGER_H__

#include <stdint.h>

#include "kernel/fs_manager.h"
#include "kernel/fd.h"

typedef int64_t (*fs_mount_op)(int64_t disk_fd, void** ctx_out);
typedef int64_t (*fs_open_op)(void* ctx, const char* path, const uint64_t flags, void** ctx_out, fd_ctx_t* fd_ctx);

typedef struct {
    fs_mount_op mount;
    fs_open_op open;
    fd_ops_t ops;
} fs_ops_t;

enum {
    FS_TYPE_UNKNOWN = 0,
    FS_TYPE_EXT2 = 1,
    FS_TYPE_SYSFS = 2,
    FS_TYPE_RAMFS = 3,
    MAX_NUM_FS_TYPE
};

void fs_manager_init(void);

int64_t fs_manager_mount_device(const char* device_name,
                                const char* path,
                                uint64_t fs_type,
                                const char* mount_name);

void fs_manager_register_filesystem(fs_ops_t* ops, uint64_t fs_type);

#endif
