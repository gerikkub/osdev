
#include <stdint.h>
#include <string.h>

#include "kernel/assert.h"
#include "kernel/fd.h"
#include "kernel/sys_device.h"
#include "kernel/vfs.h"
#include "kernel/lib/vmalloc.h"
#include "kernel/console.h"

#include "include/k_ioctl_common.h"

typedef struct {
    void* data;
    uint64_t len;
} loopback_dev_ctx_t;

typedef struct {
    loopback_dev_ctx_t* dev;

    uint64_t pos;
} loopback_file_ctx_t;

static int64_t loopback_read_op(void* ctx,
                                uint8_t* buffer,
                                const int64_t size,
                                const uint64_t flags) {
    loopback_file_ctx_t* file_ctx = ctx;

    int64_t remaining = file_ctx->dev->len - file_ctx->pos;
    int64_t read_len = remaining < size ? remaining : size;

    memcpy(buffer, file_ctx->dev->data + file_ctx->pos, read_len);
    file_ctx->pos += read_len;

    return read_len;
}

static int64_t loopback_write_op(void* ctx,
                                 const uint8_t* buffer,
                                 const int64_t size,
                                 const uint64_t flags) {
    loopback_file_ctx_t* file_ctx = ctx;

    int64_t remaining = file_ctx->dev->len - file_ctx->pos;
    int64_t write_len = remaining < size ? remaining : size;

    memcpy(file_ctx->dev->data + file_ctx->pos, buffer, write_len);
    file_ctx->pos += write_len;

    return write_len;
}

static int64_t loopback_ioctl_op(void* ctx,
                                 const uint64_t ioctl,
                                 const uint64_t* args,
                                 const uint64_t arg_count) {
    loopback_file_ctx_t* file_ctx = ctx;

    switch (ioctl) {
        case IOCTL_SEEK:
            if (arg_count != 2) {
                return -1;
            }
            int64_t pos = args[0];
            if (pos <= file_ctx->dev->len && pos >= 0) {
                file_ctx->pos = pos;
                return pos;
            } else {
                return -1;
            }

        case BLK_IOCTL_SIZE:
            return file_ctx->dev->len;
        default:
            return -1;
    }
}

static int64_t loopback_close_op(void* ctx) {
    loopback_file_ctx_t* file_ctx = ctx;
    vfree(file_ctx);
    return 0;
}

static int64_t loopback_open_op(void* ctx,
                                const char* path,
                                const uint64_t flags,
                                void** ctx_out,
                                fd_ctx_t* fd_ctx) {
    loopback_dev_ctx_t* loopback_ctx = ctx;
    loopback_file_ctx_t* file_ctx = vmalloc(sizeof(loopback_file_ctx_t));

    file_ctx->dev = loopback_ctx;
    file_ctx->pos = 0;

    *ctx_out = file_ctx;
    console_log(LOG_DEBUG, "Loopback open");
    return 0;
}

static fd_ops_t s_loopback_file_ops = {
    .read = loopback_read_op,
    .write = loopback_write_op,
    .ioctl = loopback_ioctl_op,
    .close = loopback_close_op
};

void loopback_create_raw_device(void* data, uint64_t len, const char* dev_name) {

    loopback_dev_ctx_t* loopback_ctx = vmalloc(sizeof(loopback_dev_ctx_t));
    loopback_ctx->data = data;
    loopback_ctx->len = len;

    int64_t ok = sys_device_register(&s_loopback_file_ops,
                                     loopback_open_op,
                                     loopback_ctx,
                                     dev_name);
    ASSERT(ok == 0);
}


