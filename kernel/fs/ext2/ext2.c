
#include <stdint.h>

#include "kernel/assert.h"
#include "kernel/console.h"
#include "kernel/fs_manager.h"
#include "kernel/fd.h"
#include "kernel/lib/vmalloc.h"
#include "kernel/lock/lock.h"
#include "kernel/lock/lock.h"
#include "kernel/lock/mutex.h"
#include "kernel/fs/file.h"

#include "include/k_ioctl_common.h"

#include "stdlib/bitutils.h"
#include "stdlib/string.h"

#include "kernel/fs/ext2/ext2_helpers.h"
#include "kernel/fs/ext2/ext2_structures.h"

typedef struct {
    uint32_t inode_num;
    ext2_inode_t* inode;
    lock_t inode_lock;
    ext2_fs_ctx_t* fs_ctx;
    file_ctx_t* file_ctx;
} ext2_fid_ctx_t;

typedef struct {
    llist_head_t filedata;
    ext2_inode_t* inode;
} ext2_file_hash_data_t;

static uint64_t ext2_hash_hash(void* key) {

    uint32_t* inode_key = key;

    return *inode_key;
}

static bool ext2_hash_cmp(void* key1, void* key2) {

    uint32_t* inode_key1 = key1;
    uint32_t* inode_key2 = key2;
    return *inode_key1 == *inode_key2;
}

static void ext2_hash_free(void* ctx, void* key, void* dataptr) {
    (void)ctx;
    (void)key;
    (void)dataptr;
    ASSERT(true);
}

static int64_t ext2_mount(void* disk_ctx, const fd_ops_t disk_ops, void** ctx_out) {

    ext2_fs_ctx_t* fs_ctx = vmalloc(sizeof(ext2_fs_ctx_t));

    fs_ctx->disk_ctx = disk_ctx;
    fs_ctx->disk_ops = disk_ops;

    uint8_t sb[1024];
    int64_t sb_idx = 0;
    int64_t res;
    uint64_t seek_args[2] = {1024, 0};
    res = disk_ops.ioctl(disk_ctx, IOCTL_SEEK, seek_args, 2);
    if (res < 0) {
        goto ext2_mount_failure;
    }

    res = disk_ops.read(disk_ctx, sb, 1024, 0);
    if (res < 0) {
        goto ext2_mount_failure;
    }

    memcpy(&fs_ctx->sb, sb, sizeof(ext2_superblock_t));
    sb_idx += sizeof(ext2_superblock_t);

    if (fs_ctx->sb.magic != 0xEF53 ||
        fs_ctx->sb.rev_level != EXT2_DYNAMIC_REV) {
        goto ext2_mount_failure;
    }

    memcpy(&fs_ctx->sb_ext, &sb[sb_idx], sizeof(ext2_superblock_extended_t));
    sb_idx += sizeof(ext2_superblock_extended_t);

    uint32_t bg_block;
    if (BLOCK_SIZE(fs_ctx->sb) == 1024) {
        bg_block = 2;
    } else {
        bg_block = 1;
    }

    uint32_t num_bgs = fs_ctx->sb.blocks_count / fs_ctx->sb.blocks_per_group;
    uint32_t num_bgdisc_blocks = ((num_bgs * sizeof(ext2_blockgroup_t)) + BLOCK_SIZE(fs_ctx->sb) - 1) / BLOCK_SIZE(fs_ctx->sb);
    uint8_t* bg_buffer = vmalloc(num_bgdisc_blocks * BLOCK_SIZE(fs_ctx->sb));

    for (uint64_t idx = 0; idx < num_bgdisc_blocks; idx++) {
        ext2_read_block(fs_ctx, bg_block + idx, &bg_buffer[idx*BLOCK_SIZE(fs_ctx->sb)]);
    }

    fs_ctx->bgs = (ext2_blockgroup_t*)bg_buffer;

    fs_ctx->inodes = vmalloc(num_bgs * sizeof(ext2_inode_cache_t));

    for (uint64_t idx = 0; idx < num_bgs; idx++) {
        fs_ctx->inodes[idx].valid = false;
    }

    mutex_init(&fs_ctx->fs_lock, 32);

    fs_ctx->filecache = hashmap_alloc(ext2_hash_hash,
                                      ext2_hash_cmp,
                                      ext2_hash_free,
                                      8,
                                      fs_ctx);

    *ctx_out = fs_ctx;

    return 0;

ext2_mount_failure:
    vfree(fs_ctx);
    return -1;
}

static void ext2_populate_data(void* ctx, file_data_entry_t* entry) {

    ext2_fid_ctx_t* file_ctx = ctx;

    const uint32_t block_size = BLOCK_SIZE(file_ctx->fs_ctx->sb);
    const uint64_t alloc_size = ((entry->len + block_size - 1) / block_size) * block_size;
    entry->data = vmalloc(alloc_size);

    lock_acquire(&file_ctx->inode_lock, true);
    lock_acquire(&file_ctx->fs_ctx->fs_lock, true);

    uint32_t block_num = entry->offset / block_size;

    ext2_read_inode_data(file_ctx->fs_ctx, file_ctx->inode,
                         block_num, 1, entry->data);

    lock_release(&file_ctx->fs_ctx->fs_lock);
    lock_release(&file_ctx->inode_lock);

    entry->available = 1;
}

static void ext2_flush_data(void* ctx, file_data_entry_t* entry) {
    ASSERT(0);
}

static int64_t ext2_close(void* ctx) {

    ext2_fid_ctx_t* file_ctx = ctx;

    mutex_destroy(&file_ctx->inode_lock);
    vfree(file_ctx);
    return 0;
}


static llist_head_t ext2_create_file_data(ext2_fid_ctx_t* file_ctx) {

    llist_head_t file_data = llist_create();
    
    // Only support direct and 1 indirect blocks right now...

    int idx;
    int size_remaining = file_ctx->inode->size;
    uint64_t offset = 0;
    const uint32_t block_size = BLOCK_SIZE(file_ctx->fs_ctx->sb);
    for (idx = 0; idx < 12; idx++) {
        // Break if no file data is present
        if (file_ctx->inode->block_direct[idx] == 0) {
            break;
        }

        file_data_entry_t* fd_entry = vmalloc(sizeof(file_data_entry_t));
        fd_entry->data = NULL;
        fd_entry->len = size_remaining < block_size ? size_remaining : block_size;
        fd_entry->offset = offset;
        fd_entry->dirty = 0;
        fd_entry->available = 0;

        llist_append_ptr(file_data, fd_entry);

        size_remaining -= block_size;
        offset += block_size;
    }

    if (file_ctx->inode->block_1indirect != 0) {
        uint32_t* indirect_block = vmalloc(block_size);

        ext2_read_block(file_ctx->fs_ctx,
                        file_ctx->inode->block_1indirect,
                        (void*)indirect_block);

        for (idx = 0; idx < (block_size/sizeof(uint32_t)); idx++) {
            if (indirect_block[idx] == 0) {
                break;
            }

            file_data_entry_t* fd_entry = vmalloc(sizeof(file_data_entry_t));
            fd_entry->data = NULL;
            fd_entry->len = size_remaining < block_size ? size_remaining : block_size;
            fd_entry->offset = offset;
            fd_entry->dirty = 0;
            fd_entry->available = 0;

            llist_append_ptr(file_data, fd_entry);

            size_remaining -= block_size;
            offset += block_size;
        }

        vfree(indirect_block);
    }

    return file_data;
}

static int64_t ext2_open(void* ctx, const char* file, const uint64_t flags, void** ctx_out) {

    ext2_fs_ctx_t* fs_ctx = ctx;

    lock_acquire(&fs_ctx->fs_lock, true);

    uint32_t inode_num = ext2_get_inode_for_path(fs_ctx, file);

    if (inode_num == 0) {
        return -1;
    }

    ext2_fid_ctx_t* ext2_file_ctx = vmalloc(sizeof(ext2_fid_ctx_t));
    file_ctx_t* file_ctx = vmalloc(sizeof(file_ctx_t));
    ext2_inode_t* inode = NULL;

    ext2_file_ctx->inode_num = inode_num;
    ext2_file_ctx->fs_ctx = fs_ctx;
    ext2_file_ctx->file_ctx = file_ctx;

    mutex_init(&ext2_file_ctx->inode_lock, 32);

    ext2_file_hash_data_t* ext2_hash_data = hashmap_get(fs_ctx->filecache, &inode_num);
    //ext2_file_hash_data_t* ext2_hash_data = NULL;
    if (ext2_hash_data != NULL) {
        inode = ext2_hash_data->inode;
        ext2_file_ctx->inode = inode;
        file_ctx->file_data = ext2_hash_data->filedata;
    } else {
        console_printf("Creating inode cache for %s\n", file);
        inode = vmalloc(sizeof(ext2_inode_t));
        ext2_get_inode(fs_ctx, inode_num, inode);
        ext2_file_ctx->inode = inode;
        file_ctx->file_data = ext2_create_file_data(ext2_file_ctx);

        ext2_hash_data = vmalloc(sizeof(ext2_file_hash_data_t));
        ext2_hash_data->inode = inode;
        ext2_hash_data->filedata = file_ctx->file_data;
        uint32_t* inode_key = vmalloc(sizeof(uint32_t));
        *inode_key = inode_num;
        hashmap_add(fs_ctx->filecache, inode_key, ext2_hash_data);
    }

    file_ctx->size = ext2_file_ctx->inode->size;
    file_ctx->seek_idx = 0;
    file_ctx->can_write = 0;
    file_ctx->close_op = ext2_close;
    file_ctx->populate_op = ext2_populate_data;
    file_ctx->flush_data_op = ext2_flush_data;
    file_ctx->op_ctx = ext2_file_ctx;

    *ctx_out = file_ctx;

    lock_release(&fs_ctx->fs_lock);

    return 0;
}

static fs_ops_t ext2_opts = {
    .mount = ext2_mount,
    .open = ext2_open,
    .ops = {
        .read = file_read_op,
        .write = file_write_op,
        .ioctl = file_ioctl_op,
        .close = file_close_op
    }
};

void ext2_register() {

    fs_manager_register_filesystem(&ext2_opts, FS_TYPE_EXT2);
}
