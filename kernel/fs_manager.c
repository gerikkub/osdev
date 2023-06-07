
#include <stdint.h>

#include "kernel/assert.h"
#include "kernel/console.h"
#include "kernel/vfs.h"
#include "kernel/fs_manager.h"
#include "kernel/lib/vmalloc.h"
#include "kernel/lib/llist.h"

#include "stdlib/bitutils.h"
#include "stdlib/string.h"


typedef struct {
    void* fs_ctx;
    uint64_t fs_type;
    void* disk_ctx;
    fd_ops_t disk_ops;
} fs_mount_ctx_t;

static fs_ops_t s_fs_ops[MAX_NUM_FS_TYPE];
static llist_head_t s_fs_mounts;

void fs_manager_init(void) {
    s_fs_mounts = llist_create();

    for (uint64_t idx = 0; idx < MAX_NUM_FS_TYPE; idx++) {
        s_fs_ops[idx].mount = NULL;
    }
}

int64_t fs_manager_file_open(void* ctx, const char* path, const uint64_t flags, void** ctx_out) {
    fs_mount_ctx_t* mount_ctx = ctx;

    ASSERT(s_fs_ops[mount_ctx->fs_type].open != NULL);

    return s_fs_ops[mount_ctx->fs_type].open(mount_ctx->fs_ctx, path, flags, ctx_out);
}

int64_t fs_manager_mount_device(const char* device_name,
                                const char* path,
                                uint64_t fs_type,
                                const char* mount_name) {

    int64_t res;
    fd_ops_t disk_ops;
    void* disk_ctx;

    console_log(LOG_INFO, "Mounting %s:%s at %s", device_name, path, mount_name);

    res = vfs_open_device(device_name, path, 0, &disk_ops, &disk_ctx);

    if (res < 0) {
        return res;
    }

    fs_mount_ctx_t* mount_ctx = vmalloc(sizeof(fs_mount_ctx_t));

    mount_ctx->disk_ctx = disk_ctx;
    mount_ctx->disk_ops = disk_ops;
    mount_ctx->fs_type = fs_type;

    vfs_device_ops_t vfs_device = {
        .valid = true,
        .open = fs_manager_file_open,
        .ctx = mount_ctx,
        .fd_ops = s_fs_ops[fs_type].ops,
        .name = {0}
    };

    strncpy(vfs_device.name, mount_name, MAX_DEVICE_NAME_LEN);

    res = s_fs_ops[fs_type].mount(disk_ctx, disk_ops, &mount_ctx->fs_ctx);

    if (res < 0) {
        disk_ops.close(disk_ctx);
        return res;
    }

    vfs_register_device(&vfs_device);

    console_log(LOG_INFO, "Mounted FS %d from %s:%s at %s",
                   fs_type, device_name, path, mount_name);

    return 0;
}

void fs_manager_register_filesystem(fs_ops_t* ops, uint64_t fs_type) {

    ASSERT(fs_type < MAX_NUM_FS_TYPE);
    ASSERT(fs_type != 0);

    s_fs_ops[fs_type] = *ops;
}