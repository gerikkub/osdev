
#include <stdint.h>

#include "kernel/assert.h"
#include "kernel/console.h"
#include "kernel/fs_manager.h"
#include "kernel/fd.h"
#include "kernel/lib/vmalloc.h"

#include "stdlib/bitutils.h"
#include "stdlib/string.h"

#include "kernel/fs/ext2_helpers.h"
#include "kernel/fs/ext2_structures.h"

typedef struct {
    uint64_t inode_num;
    ext2_inode_t* inode;
    uint64_t pos;
    ext2_fs_ctx_t* fs_ctx;
} ext2_fid_ctx_t;

static int64_t ext2_mount(void* disk_ctx, const fd_ops_t disk_ops, void** ctx_out) {

    ext2_fs_ctx_t* fs_ctx = vmalloc(sizeof(ext2_fs_ctx_t));

    fs_ctx->disk_ctx = disk_ctx;
    fs_ctx->disk_ops = disk_ops;

    uint8_t sb[1024];
    int64_t sb_idx = 0;
    int64_t res;
    res = disk_ops.seek(disk_ctx, 1024, 0);
    if (res < 0) {
        goto ext2_mount_failure;
    }

    res = disk_ops.read(disk_ctx, sb, 1024, 0);
    if (res < 0) {
        goto ext2_mount_failure;
    }

    memset(sb, 0, 1024);
    res = disk_ops.seek(disk_ctx, 1024, 0);
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

    *ctx_out = fs_ctx;

    return 0;

ext2_mount_failure:
    vfree(fs_ctx);
    return -1;
}

static int64_t ext2_open(void* ctx, const char* file, const uint64_t flags, void** ctx_out) {

    ext2_fs_ctx_t* fs_ctx = ctx;

    uint32_t inode_num = ext2_get_inode_for_path(fs_ctx, file);

    if (inode_num == 0) {
        return -1;
    }

    ext2_fid_ctx_t* file_ctx = vmalloc(sizeof(ext2_fid_ctx_t));
    ext2_inode_t* inode = vmalloc(sizeof(ext2_inode_t));

    file_ctx->inode_num = inode_num;
    file_ctx->inode = inode;
    file_ctx->fs_ctx = fs_ctx;
    file_ctx->pos = 0;
    ext2_get_inode(fs_ctx, inode_num, inode);

    *ctx_out = file_ctx;
    return 0;
}

static int64_t ext2_read(void* ctx, uint8_t* buffer, const int64_t size_ro, const uint64_t flags) {

    ext2_fid_ctx_t* file_ctx = ctx;
    const uint32_t block_size = BLOCK_SIZE(file_ctx->fs_ctx->sb);
    uint32_t size = size_ro;
    int32_t read_size = 0;

    // Copy first bit of data
    if (file_ctx->pos % block_size != 0) {
        uint8_t* block_buffer = vmalloc(block_size);
        uint32_t block_num = file_ctx->pos / block_size;
        uint32_t overlap = block_size - (file_ctx->pos % block_size);

        ext2_read_inode_block(file_ctx->fs_ctx, file_ctx->inode, block_num, block_buffer);

        if (size <= overlap) {
            memcpy(buffer, &block_buffer[file_ctx->pos % block_size], size);
            file_ctx->pos += size;
            vfree(block_buffer);
            return size;
        } else {
            memcpy(buffer, &block_buffer[file_ctx->pos % block_size], overlap);
            file_ctx->pos += overlap;
            read_size += overlap;
            size -= overlap;
            buffer += overlap;
            vfree(block_buffer);
        }
    }

    // Read block aligned sections
    if (size >= block_size) {
        uint32_t block_num = file_ctx->pos / block_size;
        uint32_t num_blocks = size / block_size;
        uint32_t full_block_size = num_blocks * block_size;

        ext2_read_inode_data(file_ctx->fs_ctx, file_ctx->inode,
                             block_num, num_blocks, buffer);
        read_size += full_block_size;
        file_ctx->pos += full_block_size;
        size -= full_block_size;
        buffer += full_block_size;
    }

    // Read remaining data
    if (size > 0) {
        uint8_t* block_buffer = vmalloc(block_size);
        uint32_t block_num = file_ctx->pos / block_size;

        ext2_read_inode_block(file_ctx->fs_ctx, file_ctx->inode, block_num, block_buffer);

        memcpy(buffer, block_buffer, size);
        read_size += size;
        file_ctx->pos += size;
    }

    return read_size;
}

static int64_t ext2_write(void* ctx, const uint8_t* buffer, const int64_t size, const uint64_t flags) {
    return -1;
}

static int64_t ext2_seek(void* ctx, const int64_t pos, const uint64_t flags) {
    ext2_fid_ctx_t* file_ctx = ctx;
    file_ctx->pos = pos;
    return file_ctx->pos;
}

static int64_t ext2_close(void* ctx) {
    return -1;
}

static fs_ops_t ext2_opts = {
    .mount = ext2_mount,
    .open = ext2_open,
    .ops = {
        .read = ext2_read,
        .write = ext2_write,
        .seek = ext2_seek,
        .ioctl = NULL,
        .close = ext2_close
    }
};

void ext2_register() {

    fs_manager_register_filesystem(&ext2_opts, FS_TYPE_EXT2);
}