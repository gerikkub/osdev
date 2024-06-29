
#include <stdint.h>
#include <string.h>

#include "kernel/assert.h"
#include "kernel/console.h"
#include "kernel/vfs.h"
#include "kernel/task.h"
#include "kernel/fs_manager.h"
#include "kernel/lib/vmalloc.h"
#include "kernel/lib/llist.h"

#include "stdlib/bitutils.h"


typedef struct {
    void* fs_ctx;
    uint64_t fs_type;
    int64_t disk_fd;
} fs_mount_ctx_t;

static fs_ops_t s_fs_ops[MAX_NUM_FS_TYPE];
static llist_head_t s_fs_mounts;

void fs_manager_init(void) {
    s_fs_mounts = llist_create();

    for (uint64_t idx = 0; idx < MAX_NUM_FS_TYPE; idx++) {
        s_fs_ops[idx].mount = NULL;
    }
}

int64_t fs_manager_file_open(void* ctx, const char* path, const uint64_t flags, void** ctx_out, fd_ctx_t* fd_ctx) {
    fs_mount_ctx_t* mount_ctx = ctx;

    ASSERT(s_fs_ops[mount_ctx->fs_type].open != NULL);

    return s_fs_ops[mount_ctx->fs_type].open(mount_ctx->fs_ctx, path, flags, ctx_out, fd_ctx);
}

int64_t fs_manager_mount_device(const char* device_name,
                                const char* path,
                                uint64_t fs_type,
                                const char* mount_name) {
    int64_t disk_fd;
    int64_t res;

    if (strlen(device_name) > 0 && strlen(path) > 0) {
        console_log(LOG_INFO, "Mounting %s:%s at %s", device_name, path, mount_name);

        disk_fd = vfs_open_device_fd(device_name, path, 0);

        if (disk_fd < 0) {
            console_log(LOG_DEBUG, "Unable to open device");
            return disk_fd;
        }
    } else {
        disk_fd = -1;
    }

    fs_mount_ctx_t* mount_ctx = vmalloc(sizeof(fs_mount_ctx_t));

    mount_ctx->disk_fd = disk_fd;
    mount_ctx->fs_type = fs_type;

    vfs_device_ops_t vfs_device = {
        .valid = true,
        .open = fs_manager_file_open,
        .ctx = mount_ctx,
        .fd_ops = s_fs_ops[fs_type].ops,
        .name = {0}
    };

    strncpy(vfs_device.name, mount_name, MAX_DEVICE_NAME_LEN);

    res = s_fs_ops[fs_type].mount(disk_fd, &mount_ctx->fs_ctx);

    if (res < 0) {
        fd_ctx_t* disk_fd_ctx = get_kernel_fd(disk_fd);
        fd_call_close(disk_fd_ctx);
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