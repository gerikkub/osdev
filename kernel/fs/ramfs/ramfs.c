

#include <stdint.h>
#include <string.h>

#include "kernel/fs_manager.h"
#include "kernel/task.h"
#include "kernel/kernelspace.h"
#include "kernel/kmalloc.h"
#include "kernel/lock/mutex.h"
#include "kernel/lib/vmalloc.h"
#include "kernel/lib/hashmap.h"
#include "kernel/fs/file.h"

#include "include/k_syscall.h"

typedef struct {
    hashmap_ctx_t* file_entries;
} ramfs_ctx_t;

typedef struct {
    file_data_t* file_data;
    ramfs_ctx_t* ramfs_ctx;
} ramfs_file_ctx_t;

static uint64_t ramfs_file_hm_hash(void* key) {
    uint8_t* path = key;
    uint64_t hash = 0;
    uint64_t p = 1;
    while (*path) {
        hash += ((uint64_t)*path) * p;
        p *= 31;
        path++;
    }
    return hash;
}

static bool ramfs_file_hm_cmp(void* key1, void* key2) {
    return strcmp(key1, key2) == 0;
}

static void ramfs_file_hm_free(void* ctx, void* key, void* dataptr) {
    // Note: Never freeing files in ramfs currently
    ASSERT(false);
}

static void ramfs_file_populate_data(void* ctx, file_data_entry_t* entry) {
    ramfs_file_ctx_t* ramfs_file_ctx = ctx;
    (void)ramfs_file_ctx;

    // This should never be caled for ramfs
    ASSERT(false);
}

static void ramfs_file_new_data(void* ctx, llist_head_t new_entries, uint64_t len) {
    ramfs_file_ctx_t* ramfs_file_ctx = ctx;
    (void)ramfs_file_ctx;

    // Allocate pages to fill len
    int64_t pages = PAGE_CEIL(len) / VMEM_PAGE_SIZE;

    for (int idx = 0; idx < pages; idx++) {
        file_data_entry_t* entry = vmalloc(sizeof(file_data_entry_t));

        entry->data = PHY_TO_KSPACE_PTR(kmalloc_phy(VMEM_PAGE_SIZE));
        memset(entry->data, 0, VMEM_PAGE_SIZE);
        entry->len = VMEM_PAGE_SIZE;
        entry->offset = 0;
        entry->ctx = NULL;
        entry->dirty = 0;
        entry->available = 1;

        llist_append_ptr(new_entries, entry);
    }
}

static void ramfs_file_flush_data(void* ctx, file_data_entry_t* entry) {
    ramfs_file_ctx_t* ramfs_file_ctx = ctx;
    (void)ramfs_file_ctx;

    // No-op
}

static int64_t ramfs_file_close(void* ctx) {
    ramfs_file_ctx_t* ramfs_file_ctx = ctx;
    (void)ramfs_file_ctx;

    // No-op
    return 0;
}

static int64_t ramfs_mount(int64_t disk_fd, void** ctx_out) {
    ramfs_ctx_t* ramfs_ctx = vmalloc(sizeof(ramfs_ctx_t));

    ramfs_ctx->file_entries = hashmap_alloc(ramfs_file_hm_hash,
                                            ramfs_file_hm_cmp,
                                            ramfs_file_hm_free,
                                            9,
                                            ramfs_ctx);

    *ctx_out = ramfs_ctx;

    return 0;
}

static int64_t ramfs_open(void* ctx, const char* path, const uint64_t flags, void** ctx_out, fd_ctx_t* fd_ctx) {
    ramfs_ctx_t* ramfs_ctx = ctx;
    
    task_t* task = get_active_task();

    fd_ctx->ops.read = file_read_op;
    fd_ctx->ops.write = file_write_op;
    fd_ctx->ops.ioctl = file_ioctl_op;
    fd_ctx->ops.close = file_close_op;
    fd_ctx->ready = 0;
    fd_ctx->task = task;

    ramfs_file_ctx_t* ramfs_file_ctx = NULL;
    if (flags & SYSCALL_OPEN_CREATE) {
        file_data_t* file_data = vmalloc(sizeof(file_data_t));
        file_data->data_list = llist_create();
        file_data->size = 0;
        file_data->ref_count = 0;
        mutex_init(&file_data->ref_lock, 16);
        file_data->close_op = ramfs_file_close;
        file_data->populate_op = ramfs_file_populate_data;
        file_data->new_data_op = ramfs_file_new_data;
        file_data->flush_data_op = ramfs_file_flush_data;
        file_data->op_ctx = ramfs_file_ctx;

        ramfs_file_ctx = vmalloc(sizeof(ramfs_file_ctx_t));
        ramfs_file_ctx->file_data = file_data;
        ramfs_file_ctx->ramfs_ctx = ramfs_ctx;
    } else {
        ramfs_file_ctx = hashmap_get(ramfs_ctx->file_entries, (void*)path);

        if (ramfs_file_ctx == NULL) {
            return -1;
        }
    }
    ramfs_file_ctx->file_data->ref_count++;

    file_ctx_t* file_ctx = vmalloc(sizeof(file_ctx_t));
    file_ctx->file_data = ramfs_file_ctx->file_data;
    file_ctx->seek_idx = 0;
    file_ctx->can_write = (flags & SYSCALL_OPEN_WRITE) != 0;
    file_ctx->fd_ctx = fd_ctx;

    fd_ctx->ctx = file_ctx;

    *ctx_out = file_ctx;

    fd_ctx->valid = true;
    return 0;
}

static fs_ops_t ramfs_opts = {
    .mount = ramfs_mount,
    .open = ramfs_open,
    .ops = {
        .read = file_read_op,
        .write = file_write_op,
        .ioctl = file_ioctl_op,
        .close = file_close_op
    }
};

void ramfs_register() {

    fs_manager_register_filesystem(&ramfs_opts, FS_TYPE_RAMFS);
}
