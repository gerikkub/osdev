
#include <stdint.h>

#include <kernel/lib/llist.h>
#include <kernel/lib/vmalloc.h>
#include <kernel/fd.h>
#include <kernel/vfs.h>

#include <kernel/fs/sysfs/sysfs.h>

#include <stdlib/string.h>
#include <stdlib/bitutils.h>

#define SYSFS_FILENAME_MAX 128

typedef struct {
    char filename[SYSFS_FILENAME_MAX];
    sysfs_open_op open;
    fd_ops_t ops;
} sysfs_file_t;


static llist_head_t g_sysfs_files = NULL;

void sysfs_create_file(char* name, sysfs_open_op open_op, fd_ops_t* ops) {

    sysfs_file_t* f_new = vmalloc(sizeof(sysfs_file_t));

    strncpy((char*)&f_new->filename, name, SYSFS_FILENAME_MAX);
    f_new->open = open_op;
    f_new->ops = *ops;

    llist_append_ptr(g_sysfs_files, f_new);
}

int64_t sysfs_open(void* ctx, const char* path, const uint64_t flags, void** ctx_out) {

    sysfs_file_t* file = NULL;
    bool found = false;
    FOR_LLIST(g_sysfs_files, file)
        if (strncmp(path, file->filename, SYSFS_FILENAME_MAX) == 0) {
            found = true;
            break;
        }
    END_FOR_LLIST()

    if (!found) {
        return -1;
    }

    sysfs_file_fd_ctx_t* fd_ctx = vmalloc(sizeof(sysfs_file_fd_ctx_t));
    fd_ctx->ops = file->ops;

    if(file->open != NULL) {
        fd_ctx->ctx = file->open();
    } else {
        fd_ctx->ctx = NULL;
    }

    *ctx_out = fd_ctx;

    return 0;
}

int64_t sysfs_read(void* ctx, uint8_t* buffer, const int64_t size, const uint64_t flags) {

    sysfs_file_fd_ctx_t* file_ctx = ctx;
    if (file_ctx->ops.read != NULL)  {
        return file_ctx->ops.read(file_ctx->ctx, buffer, size, flags);
    } else {
        return -1;
    }

}

int64_t sysfs_write(void* ctx, const uint8_t* buffer, const int64_t size, const uint64_t flags) {

    sysfs_file_fd_ctx_t* file_ctx = ctx;
    if (file_ctx->ops.write != NULL)  {
        return file_ctx->ops.write(file_ctx->ctx, buffer, size, flags);
    } else {
        return -1;
    }
}

int64_t sysfs_ioctl(void* ctx, const uint64_t ioctl, const uint64_t* args, const uint64_t arg_count) {

    sysfs_file_fd_ctx_t* file_ctx = ctx;
    if (file_ctx->ops.ioctl != NULL)  {
        return file_ctx->ops.ioctl(file_ctx->ctx, ioctl, args, arg_count);
    } else {
        return -1;
    }
}

int64_t sysfs_close(void* ctx) {

    sysfs_file_fd_ctx_t* file_ctx = ctx;
    if (file_ctx->ops.ioctl != NULL)  {
        return file_ctx->ops.close(file_ctx->ctx);
    }

    vfree(ctx);

    return 0;
}


void sysfs_register() {

    vfs_device_ops_t vfs_sysfs_ops = {
        .valid = true,
        .open = sysfs_open,
        .ctx = NULL,
        .fd_ops = {
            .read = sysfs_read,
            .write = sysfs_write,
            .ioctl = sysfs_ioctl,
            .close = sysfs_close
        },
        .name = "sysfs"
    };

    vfs_register_device(&vfs_sysfs_ops);

    g_sysfs_files = llist_create();

    sysfs_task_init();
}