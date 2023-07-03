
#include <stdint.h>

#include "kernel/assert.h"
#include "kernel/console.h"
#include "kernel/fd.h"
#include "kernel/vfs.h"
#include "kernel/sys_device.h"
#include "kernel/lib/vmalloc.h"

#include "stdlib/bitutils.h"
#include <string.h>

#define MAX_NUM_SYS_DEVICES 64

typedef struct {
    bool valid;
    device_open_op open;
    void* ctx;
    fd_ops_t fd_ops;
    char name[MAX_SYS_DEVICE_NAME_LEN];
} sys_device_ctx_t;


typedef struct {
    int64_t sys_device_idx;
    void* open_ctx;
}  sys_device_open_ctx_t;

static sys_device_ctx_t s_sys_devices[MAX_NUM_SYS_DEVICES];

int64_t sys_device_open(void* ctx, const char* path, const uint64_t flags, void** ctx_out, fd_ctx_t* fd_ctx) {

    for (int64_t idx = 0; idx < MAX_NUM_SYS_DEVICES; idx++) {
        if (s_sys_devices[idx].valid &&
            strncmp(s_sys_devices[idx].name, path, MAX_NUM_SYS_DEVICES) == 0) {

            void* open_ctx;
            int64_t res = s_sys_devices[idx].open(s_sys_devices[idx].ctx, NULL, flags, &open_ctx, fd_ctx);
            if (res >= 0) {

                sys_device_open_ctx_t* dev_ctx = vmalloc(sizeof(sys_device_open_ctx_t));
                dev_ctx->sys_device_idx = idx;
                dev_ctx->open_ctx = open_ctx;

                *ctx_out = (void*)dev_ctx;
            }

            return res;
        }
    }

    return -1;
}

int64_t sys_device_read(void* ctx, uint8_t* buffer, const int64_t size, const uint64_t flags) {
    sys_device_open_ctx_t* dev_ctx = ctx;
    if (s_sys_devices[dev_ctx->sys_device_idx].fd_ops.read != NULL) {
        return s_sys_devices[dev_ctx->sys_device_idx].fd_ops.read(dev_ctx->open_ctx, buffer, size, flags);
    } else {
        return -1;
    }
}

int64_t sys_device_write(void* ctx, const uint8_t* buffer, const int64_t size, const uint64_t flags) {
    sys_device_open_ctx_t* dev_ctx = ctx;
    if (s_sys_devices[dev_ctx->sys_device_idx].fd_ops.write != NULL) {
        return s_sys_devices[dev_ctx->sys_device_idx].fd_ops.write(dev_ctx->open_ctx, buffer, size, flags);
    } else {
        return -1;
    }
}

int64_t sys_device_ioctl(void* ctx, const uint64_t ioctl, const uint64_t* args, const uint64_t arg_count) {
    sys_device_open_ctx_t* dev_ctx = ctx;
    if (s_sys_devices[dev_ctx->sys_device_idx].fd_ops.ioctl != NULL) {
        return s_sys_devices[dev_ctx->sys_device_idx].fd_ops.ioctl(dev_ctx->open_ctx, ioctl, args, arg_count);
    } else {
        return -1;
    }
}

int64_t sys_device_close(void* ctx) {
    sys_device_open_ctx_t* dev_ctx = ctx;
    if (s_sys_devices[dev_ctx->sys_device_idx].fd_ops.close != NULL) {
        s_sys_devices[dev_ctx->sys_device_idx].fd_ops.close(dev_ctx->open_ctx);
    }

    vfree(dev_ctx);
    return 0;
}

static vfs_device_ops_t s_sys_device_ops = {
    .valid = true,
    .open = sys_device_open,
    .ctx = NULL,
    .fd_ops = {
        .read = sys_device_read,
        .write = sys_device_write,
        .ioctl = sys_device_ioctl,
        .close = sys_device_close
    },
    .name = "sys"
};

void sys_device_init(void) {
    vfs_register_device(&s_sys_device_ops);
}

int64_t sys_device_register(fd_ops_t* ops, device_open_op open_op, void* ctx, const char* name) {

    // Check if the device already exists first
    for (int64_t idx = 0; idx < MAX_NUM_SYS_DEVICES; idx++) {
        if (s_sys_devices[idx].valid) {
            ASSERT(strncmp(s_sys_devices[idx].name, name, MAX_SYS_DEVICE_NAME_LEN) != 0);
        }
    }

    for (int64_t idx = 0; idx < MAX_NUM_SYS_DEVICES; idx++) {
        if (!s_sys_devices[idx].valid) {
            s_sys_devices[idx].valid = true;
            s_sys_devices[idx].open = open_op;
            s_sys_devices[idx].ctx = ctx;
            s_sys_devices[idx].fd_ops = *ops;
            strncpy(s_sys_devices[idx].name, name, MAX_SYS_DEVICE_NAME_LEN);

            return 0;
        }
    }

    return -1;
}