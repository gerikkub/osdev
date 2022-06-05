

#include <stdint.h>
#include <stdbool.h>

#include "kernel/assert.h"
#include "kernel/fd.h"
#include "kernel/lib/vmalloc.h"

#include "kernel/fs/ext2/ext2_helpers.h"
#include "kernel/fs/ext2/ext2_structures.h"

#include "include/k_ioctl_common.h"

#include "stdlib/bitutils.h"
#include "stdlib/string.h"


void ext2_get_inode_idx(ext2_superblock_t* sb, const uint32_t inode,
                        uint32_t* bg_out, uint32_t* ino_idx_out) {
    ASSERT(bg_out != NULL);
    ASSERT(ino_idx_out != NULL);

    *bg_out = (inode - 1) / sb->inodes_per_group;
    *ino_idx_out = (inode - 1) % sb->inodes_per_group;
}

void ext2_disk_read(ext2_fs_ctx_t* fs, uint64_t offset, void* buffer, uint64_t size) {

    int64_t res;
    uint64_t seek_args[2] = {offset, 0};
    res = fs->disk_ops.ioctl(fs->disk_ctx, IOCTL_SEEK, seek_args, 2);
    ASSERT(res >= 0);

    res = fs->disk_ops.read(fs->disk_ctx, buffer, size, 0);
    ASSERT(res >= 0);
}

void ext2_read_block(ext2_fs_ctx_t* fs, const uint64_t block, void* buffer) {

    if (block != 0) {
        uint64_t block_offset = block * BLOCK_SIZE(fs->sb);

        ext2_disk_read(fs, block_offset, buffer, BLOCK_SIZE(fs->sb));
    } else {
        memset(buffer, 0, BLOCK_SIZE(fs->sb));
    }
}

void ext2_read_blocks(ext2_fs_ctx_t* fs,
                      const uint64_t block_start,
                      const uint64_t num_blocks,
                      void* buffer) {

    ASSERT(block_start != 0);

    uint64_t block_offset = block_start * BLOCK_SIZE(fs->sb);

    ext2_disk_read(fs, block_offset, buffer, num_blocks * BLOCK_SIZE(fs->sb));
}

void ext2_populate_inode_cache(ext2_fs_ctx_t* fs, const uint32_t bg) {

    uint64_t num_inode_blocks = (fs->sb.inodes_per_group * fs->sb_ext.inode_size) / BLOCK_SIZE(fs->sb);

    if (fs->inodes[bg].inodes == NULL) {
        fs->inodes[bg].inodes = vmalloc(num_inode_blocks * BLOCK_SIZE(fs->sb));
    }
    
    ext2_read_blocks(fs, fs->bgs[bg].inode_table, num_inode_blocks, fs->inodes[bg].inodes);
    fs->inodes[bg].valid = true;
}

uint64_t ext2_get_inode_size(ext2_fs_ctx_t* fs, const ext2_inode_t* inode) {

    return ((uint64_t)inode->size) +
           (((uint64_t)inode->dir_acl) << 32);
}


void ext2_read_inode_block(ext2_fs_ctx_t* fs, const ext2_inode_t* inode,
                           const uint32_t block_num, uint8_t* buffer) {

    const uint32_t num_direct_blocks = 12;
    const uint32_t num_1indirect_blocks = BLOCK_SIZE(fs->sb) / sizeof(uint32_t);
    const uint32_t num_2indirect_blocks = num_1indirect_blocks * num_1indirect_blocks;
    const uint32_t num_3indirect_blocks = num_2indirect_blocks * num_1indirect_blocks;

    if (block_num < num_direct_blocks) {
        // Direct block
        ext2_read_block(fs, inode->block_direct[block_num], buffer);
    } else if (block_num < num_1indirect_blocks) {
        uint32_t* block_buffer = vmalloc(BLOCK_SIZE(fs->sb));

        uint32_t block_idx = block_num - num_direct_blocks;

        ext2_read_block(fs, inode->block_1indirect, block_buffer);

        ext2_read_block(fs, block_buffer[block_idx], buffer);

        vfree(block_buffer);
    } else if (block_num < num_2indirect_blocks) {
        uint32_t* block_buffer_1 = vmalloc(BLOCK_SIZE(fs->sb));
        uint32_t* block_buffer_2 = vmalloc(BLOCK_SIZE(fs->sb));

        uint32_t block_idx = block_num - num_direct_blocks - num_1indirect_blocks;

        uint32_t block_idx_1 = block_idx / num_1indirect_blocks;
        uint32_t block_idx_2 = block_idx % num_1indirect_blocks;

        ext2_read_block(fs, inode->block_2indirect, block_buffer_1);

        ext2_read_block(fs, block_buffer_1[block_idx_1], block_buffer_2);

        ext2_read_block(fs, block_buffer_2[block_idx_2], buffer);

        vfree(block_buffer_1);
        vfree(block_buffer_2);

    } else if (block_num < num_3indirect_blocks) {
        uint32_t* block_buffer_1 = vmalloc(BLOCK_SIZE(fs->sb));
        uint32_t* block_buffer_2 = vmalloc(BLOCK_SIZE(fs->sb));
        uint32_t* block_buffer_3 = vmalloc(BLOCK_SIZE(fs->sb));

        uint32_t block_idx = block_num - num_direct_blocks
                                       - num_1indirect_blocks
                                       - num_2indirect_blocks;

        uint32_t block_idx_1 = block_idx / num_2indirect_blocks;
        uint32_t block_idx_2 = block_idx / num_1indirect_blocks;
        uint32_t block_idx_3 = block_idx % num_1indirect_blocks;

        ext2_read_block(fs, inode->block_3indirect, block_buffer_1);

        ext2_read_block(fs, block_buffer_1[block_idx_1], block_buffer_2);

        ext2_read_block(fs, block_buffer_2[block_idx_2], block_buffer_3);

        ext2_read_block(fs, block_buffer_3[block_idx_3], buffer);

        vfree(block_buffer_1);
        vfree(block_buffer_2);
        vfree(block_buffer_3);
    } else {
        ASSERT(0);
    }
}

void ext2_read_inode_data(ext2_fs_ctx_t* fs, const ext2_inode_t* inode,
                          const uint32_t block_start, const uint32_t num_blocks,
                          uint8_t* buffer) {

    for (uint32_t idx = 0; idx < num_blocks; idx++) {
        uint32_t block = block_start + idx;

        ext2_read_inode_block(fs, inode, block, &buffer[idx * BLOCK_SIZE(fs->sb)]);
    }
}

void ext2_get_inode(ext2_fs_ctx_t* fs, const uint32_t inode_num, ext2_inode_t* inode_out) {

    uint32_t inode_bg, inode_idx;

    ext2_get_inode_idx(&fs->sb, inode_num, &inode_bg, &inode_idx);

    if (!fs->inodes[inode_bg].valid) {
        ext2_populate_inode_cache(fs, inode_bg);
    }

    ASSERT(fs->inodes[inode_bg].valid);
    ASSERT(fs->inodes[inode_bg].inodes != NULL);

    ext2_inode_t* inode = fs->inodes[inode_bg].inodes + (fs->sb_ext.inode_size * inode_idx);
    *inode_out = *inode;
}

uint32_t ext2_get_inode_in_dir(ext2_fs_ctx_t* fs, const ext2_inode_t* inode, const char* name) {

    uint8_t* block_buffer = vmalloc(BLOCK_SIZE(fs->sb));
    uint8_t name_len = strnlen(name, 256);

    uint32_t block_num = 0;
    uint64_t inode_size = ext2_get_inode_size(fs, inode);
    ext2_dir_entry_t* entry;
    while ((block_num * BLOCK_SIZE(fs->sb)) < (inode_size)) {
        uint32_t block_idx = 0;
        ext2_read_inode_data(fs, inode, block_idx, 1, block_buffer);
        while (block_idx < BLOCK_SIZE(fs->sb)) {
            entry = (ext2_dir_entry_t*)&block_buffer[block_idx];
            ASSERT(entry != NULL);

            if (entry->inode != 0 &&
                entry->name_len == name_len &&
                strncmp(entry->name, name, name_len) == 0) {
                
                goto found_inode;
            }

            ASSERT(entry->rec_len > 0);
            block_idx += entry->rec_len;
        }

        block_num++;
    }

    vfree(block_buffer);

    return 0;

found_inode:
    vfree(block_buffer);

    return entry->inode;
}

uint32_t ext2_get_inode_for_path_rel(ext2_fs_ctx_t* fs, const ext2_inode_t* inode_start,
                                 const char* path) {

    const uint32_t path_len = strnlen(path, 65536) + 1;
    char* path_copy = vmalloc(path_len);
    memcpy(path_copy, path, path_len);

    uint64_t path_idx = 0;
    uint64_t path_sep_idx = 0;

    bool last_path = false;

    ext2_inode_t curr_inode = *inode_start;
    uint32_t next_inode_num;

    while (!last_path) {
        while (!(path_copy[path_sep_idx] == '/' ||
                 path_copy[path_sep_idx] == '\0')) {
            path_sep_idx++;
        }

        if (path_copy[path_sep_idx] == '\0') {
            last_path = true;
        }
        path_copy[path_sep_idx] = '\0';

        // Do any of the inodes match the name?
        next_inode_num = ext2_get_inode_in_dir(fs,
                                               &curr_inode,
                                               &path_copy[path_idx]);

        if (next_inode_num == 0 ||
            last_path) {
            break;
        }

        ext2_get_inode(fs, next_inode_num, &curr_inode);

        path_idx = path_sep_idx + 1;
    }

    return last_path ? next_inode_num : 0;
}

uint32_t ext2_get_inode_for_path(ext2_fs_ctx_t* fs, const char* path) {

    // Find root inode
    ext2_inode_t root_inode;
    
    ext2_get_inode(fs, EXT2_ROOT_INODE_NUM, &root_inode);

    if (path[0] == '/') {
        path += 1;
    }

    return ext2_get_inode_for_path_rel(fs, &root_inode, path);
}