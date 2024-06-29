
#include <stdint.h>
#include <string.h>

#include "kernel/assert.h"
#include "kernel/console.h"
#include "kernel/vfs.h"
#include "kernel/panic.h"
#include "kernel/task.h"

#include "stdlib/bitutils.h"

#define MAX_NUM_DEVICES 64

static vfs_device_ops_t s_devices[MAX_NUM_DEVICES] = {0};

void vfs_register_device(vfs_device_ops_t* device) {

    ASSERT(device != NULL);

    ASSERT(device->name[MAX_DEVICE_NAME_LEN-1] == 0);

    console_log(LOG_INFO, "Registering %s FS", device->name);

    int idx;
    for (idx = 0; idx < MAX_NUM_DEVICES; idx++) {
        if (!s_devices[idx].valid) {
            s_devices[idx] = *device;
            s_devices[idx].valid = true;
            break;
        }
    }

    if (idx >= MAX_NUM_DEVICES) {
        PANIC("Unable to register %s FS", device->name);
    }
}

static int64_t vfs_open_device(const char* device_name, const char* path, uint64_t flags, fd_ops_t* ops_out, void** ctx_out, fd_ctx_t* fd_ctx) {
    ASSERT(device_name != NULL);
    ASSERT(path != NULL);
    ASSERT(ops_out != NULL);
    ASSERT(ctx_out != NULL);

    for (int idx = 0; idx < MAX_NUM_DEVICES; idx++) {
        if (s_devices[idx].valid &&
            strncmp(s_devices[idx].name, device_name, MAX_DEVICE_NAME_LEN) == 0) {

            int64_t res = s_devices[idx].open(s_devices[idx].ctx, path, flags, ctx_out, fd_ctx);
            if (res >= 0) {
                *ops_out = s_devices[idx].fd_ops;
            }
            return res;
        }
    }

    console_log(LOG_DEBUG, "VFS can not find %s:%s", device_name, path);
    return -1;
}

int64_t vfs_open_device_fd(const char* device_name, const char* path, uint64_t flags) {
    task_t* task = get_active_task();

    int64_t fd = find_open_fd(task);
    if (fd < 0) {
        return -1;
    }

    fd_ctx_t* fd_ctx = get_task_fd(fd, task);
    fd_ctx->valid = true;
    fd_ctx->task = task;

    int64_t stat = vfs_open_device(device_name, path, flags,
                                   &fd_ctx->ops,
                                   &fd_ctx->ctx,
                                   fd_ctx);

    fd_ctx->valid = stat >= 0;
    return stat >= 0 ? fd : -1;
}
