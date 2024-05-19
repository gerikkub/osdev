
#include <stdint.h>
#include <string.h>

#include "kernel/assert.h"
#include "kernel/console.h"
#include "kernel/lib/llist.h"
#include "kernel/lib/vmalloc.h"
#include "kernel/drivers.h"
#include "kernel/fd.h"
#include "kernel/sys_device.h"
#include "kernel/task.h"
#include "kernel/kmalloc.h"

#include "drivers/spi/spi.h"

#include "include/k_ioctl_common.h"
#include "include/k_spi.h"

#include "stdlib/bitutils.h"
#include "stdlib/printf.h"

typedef struct spi_device_ctx_ {
    void* driver_ctx;
    spi_ops_t* spi_ops;

    k_spi_device_t device_args;

    uint64_t* fd_ready;

    spi_txn_t* active_txn;
    bool txn_complete;
} spi_device_ctx_t;

void spi_txn_complete(spi_txn_t* txn) {
    txn->device_ctx->txn_complete = true;
}

static void spi_cleanup_txn(spi_txn_t* txn) {
    vfree(txn->write_mem);
    vfree(txn->read_mem);
    vfree(txn);
}

int64_t spi_device_read(void* ctx, uint8_t* buffer, const int64_t size, const uint64_t flags) {
    spi_device_ctx_t* spi_ctx = ctx;

    if (spi_ctx->active_txn == NULL ||
        spi_ctx->active_txn->len != size) {
        return -1;
    }

    if (!spi_ctx->txn_complete) {
        return 0;
    }

    memcpy(buffer, spi_ctx->active_txn->read_mem, spi_ctx->active_txn->len);

    spi_cleanup_txn(spi_ctx->active_txn);
    spi_ctx->active_txn = NULL;

    return size;
}

int64_t spi_device_write(void* ctx, const uint8_t* buffer, const int64_t size, const uint64_t flags) {
    spi_device_ctx_t* spi_ctx = ctx;

    if (spi_ctx->active_txn != NULL) {
        if (!spi_ctx->txn_complete) {
            return -1;
        }
        spi_cleanup_txn(spi_ctx->active_txn);
        spi_ctx->active_txn = NULL;
    }

    uint8_t* write_mem = vmalloc(size);
    uint8_t* read_mem = vmalloc(size);

    memcpy(write_mem, buffer, size);


    spi_txn_t* txn = vmalloc(sizeof(spi_txn_t));
    txn->len = size;
    txn->write_pos = 0;
    txn->write_mem = write_mem;
    txn->read_pos = 0;
    txn->read_mem = read_mem;
    txn->device_ctx = spi_ctx;

    spi_ctx->txn_complete = false;
    spi_ctx->active_txn = txn;

    return spi_ctx->spi_ops->execute_fn(spi_ctx->driver_ctx, &spi_ctx->device_args, txn);
}

int64_t spi_device_ioctl(void* ctx, const uint64_t ioctl, const uint64_t* args, const uint64_t arg_count) {
    return -1;
}

int64_t spi_device_close(void* ctx) {
    spi_device_ctx_t* spi_ctx = ctx;
    if (spi_ctx->txn_complete) {
        if (spi_ctx->active_txn != NULL) {
            spi_cleanup_txn(spi_ctx->active_txn);
        }
    }
    vfree(spi_ctx);

    return 0;
}

static fd_ops_t s_spi_device_ops = {
    .read = spi_device_read,
    .write = spi_device_write,
    .ioctl = spi_device_ioctl,
    .close = spi_device_close,
};

int64_t spi_create_device(void* driver_ctx,
                        spi_ops_t* ops,
                        k_spi_device_t* new_device) {

    console_log(LOG_INFO, "Creating SPI Device");

    task_t* task = get_active_task();
    int64_t fd_num = find_open_fd(task);
    if (fd_num < 0) {
        return -1;
    }

    spi_device_ctx_t* device_ctx = vmalloc(sizeof(spi_device_ctx_t));
    device_ctx->driver_ctx = driver_ctx;
    device_ctx->spi_ops = ops;
    device_ctx->device_args = *new_device;

    fd_ctx_t* fd = get_task_fd(fd_num, task);
    fd->ctx = device_ctx;
    fd->ops = s_spi_device_ops;
    fd->ready = 0;
    fd->valid = true;

    return fd_num;
}