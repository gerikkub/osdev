

#include <stdint.h>
#include <stdbool.h>

#include "kernel/assert.h"
#include "kernel/fd.h"
#include "kernel/console.h"
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
    ASSERT(inode < sb->inodes_count);

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


uint32_t ext2_get_inode_block_num(ext2_fs_ctx_t* fs, const ext2_inode_t* inode, const uint32_t block_num) {

    const uint32_t num_direct_blocks = EXT2_BLOCK_DIRECT_MAX(fs);
    const uint32_t num_1indirect_blocks = EXT2_BLOCK_1INDIRECT_MAX(fs);
    const uint32_t num_2indirect_blocks = EXT2_BLOCK_2INDIRECT_MAX(fs);
    const uint32_t num_3indirect_blocks = EXT2_BLOCK_3INDIRECT_MAX(fs);

    uint32_t disk_block_num = 0;

    if (block_num < num_direct_blocks) {
        // Direct block
        disk_block_num = inode->block_direct[block_num];
    } else if (block_num < num_1indirect_blocks) {
        uint32_t* block_buffer = vmalloc(BLOCK_SIZE(fs->sb));

        uint32_t block_idx = block_num - num_direct_blocks;

        ext2_read_block(fs, inode->block_1indirect, block_buffer);

        disk_block_num = block_buffer[block_idx];

        vfree(block_buffer);
    } else if (block_num < num_2indirect_blocks) {
        uint32_t* block_buffer_1 = vmalloc(BLOCK_SIZE(fs->sb));
        uint32_t* block_buffer_2 = vmalloc(BLOCK_SIZE(fs->sb));

        uint32_t block_idx = block_num - num_direct_blocks - num_1indirect_blocks;

        uint32_t block_idx_1 = block_idx / num_1indirect_blocks;
        uint32_t block_idx_2 = block_idx % num_1indirect_blocks;

        ext2_read_block(fs, inode->block_2indirect, block_buffer_1);

        ext2_read_block(fs, block_buffer_1[block_idx_1], block_buffer_2);

        disk_block_num = block_buffer_2[block_idx_2];

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

        disk_block_num = block_buffer_3[block_idx_3];

        vfree(block_buffer_1);
        vfree(block_buffer_2);
        vfree(block_buffer_3);
    } else {
        ASSERT(0);
    }

    return disk_block_num;
}

void ext2_read_inode_block(ext2_fs_ctx_t* fs, const ext2_inode_t* inode,
                           const uint32_t block_num, uint8_t* buffer) {

    uint32_t disk_block_num = ext2_get_inode_block_num(fs, inode, block_num);

    ext2_read_block(fs, disk_block_num, buffer);
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

    vfree(path_copy);

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

void ext2_disk_write(ext2_fs_ctx_t* fs, const uint64_t offset, const void* buffer, uint64_t size) {

    int64_t res;
    uint64_t seek_args[2] = {offset, 0};
    res = fs->disk_ops.ioctl(fs->disk_ctx, IOCTL_SEEK, seek_args, 2);
    ASSERT(res >= 0);

    res = fs->disk_ops.write(fs->disk_ctx, buffer, size, 0);
    ASSERT(res >= 0);

}

void ext2_write_block(ext2_fs_ctx_t* fs, const uint64_t block, const void* buffer) {

    uint64_t block_offset = block * BLOCK_SIZE(fs->sb);

    ext2_disk_write(fs, block_offset, buffer, BLOCK_SIZE(fs->sb));
}

void ext2_write_blocks(ext2_fs_ctx_t* fs,
                       const uint64_t block_start,
                       const uint64_t num_blocks,
                       void* buffer) {

    const uint32_t block_size = BLOCK_SIZE(fs->sb);

    for (int64_t idx = 0; idx < num_blocks; idx++) {
        ext2_write_block(fs, block_start + idx, buffer + (idx* block_size));
    }
}

static int64_t ext2_alloc_block_in_bitmap(uint8_t* bitmap_data, uint64_t block_size) {

    for (int64_t idx = 0; idx < block_size; idx++) {
        uint8_t byte_val = bitmap_data[idx];
        int64_t bit_idx = 0;
        if (byte_val != 0xFF) {
            for (bit_idx = 0; bit_idx < 8; bit_idx++) {
                if ((~byte_val) & (1 << bit_idx)) {
                    break;
                }
            }

            bitmap_data[idx] |= (1 << bit_idx);
            return idx * 8 + bit_idx;
        }
    }

    return -1;
}

uint32_t ext2_alloc_block(ext2_fs_ctx_t* fs) {
    
    uint32_t block_size = BLOCK_SIZE(fs->sb);
    uint32_t num_bgs = fs->sb.blocks_count / fs->sb.blocks_per_group;

    // Loop through each block group
    uint64_t bg_idx = 0;
    for (bg_idx = 0; bg_idx < num_bgs; bg_idx++) {
        if (fs->bgs[bg_idx].free_blocks_count > 0) {
            break;
        }
    }

    // Return 0 on a full disk
    if (bg_idx >= num_bgs) {
        return 0;
    }

    uint32_t num_bitmap_blocks = fs->sb.blocks_per_group;
    uint8_t* b_bitmap_data = vmalloc(block_size);
    int64_t new_block = -1;

    for (uint64_t idx = 0; idx < num_bitmap_blocks; idx++) {
        ext2_read_block(fs, fs->bgs[bg_idx].block_bitmap + idx, b_bitmap_data);

        new_block = ext2_alloc_block_in_bitmap(b_bitmap_data, block_size);

        if (new_block >= 0) {
            new_block += idx*(8*block_size) + fs->sb.first_data_block;

            ext2_write_block(fs, fs->bgs[bg_idx].block_bitmap + idx, b_bitmap_data);
            break;
        }
    }

    ASSERT(new_block > 0);
    ASSERT(new_block <= UINT32_MAX);

    fs->bgs[bg_idx].free_blocks_count--;
    fs->sb.free_blocks_count--;
    fs->bgs_dirty = true;
    fs->sb_dirty = true;

    return (uint32_t)new_block;
}

uint32_t ext2_alloc_block_to_inode(ext2_fs_ctx_t* fs, ext2_inode_t* inode) {

    // Get a block
    uint32_t new_block = ext2_alloc_block(fs);

    uint32_t block_size = BLOCK_SIZE(fs->sb);
    uint32_t blocks_div_512 = block_size / 512;
    uint32_t inode_full_blocks = inode->blocks / blocks_div_512;

    // Only support writing up to the 1st indirect block for now
    ASSERT(inode_full_blocks < EXT2_BLOCK_2INDIRECT_MAX(fs));

    if (inode_full_blocks < EXT2_BLOCK_DIRECT_MAX(fs)) {
        // Allocate to a direct block
        inode->block_direct[inode_full_blocks] = new_block;
    } else if (inode_full_blocks < EXT2_BLOCK_1INDIRECT_MAX(fs)) {

        uint32_t* ind_block_ptr = vmalloc(block_size);
        if (inode->block_1indirect == 0) {
            uint32_t indirect_block = ext2_alloc_block(fs);
            inode->block_1indirect = indirect_block;

            memset(ind_block_ptr, 0, block_size);
        } else {
            ext2_read_block(fs, inode->block_1indirect, ind_block_ptr);
        }

        uint32_t ind_block_offset = inode_full_blocks - EXT2_BLOCK_1INDIRECT_MAX(fs);

        ind_block_ptr[ind_block_offset] = new_block;

        ext2_write_block(fs, inode->block_1indirect, ind_block_ptr);

        vfree(ind_block_ptr);
    } else {
        ASSERT(0);
    }

    inode->blocks *= blocks_div_512;

    return new_block;
}

void ext2_mark_sb_dirty(ext2_fs_ctx_t* fs) {
    fs->sb_dirty = true;
}

void ext2_flush_inode(ext2_fs_ctx_t* fs, const uint32_t inode_num, const ext2_inode_t* inode_data) {

    uint32_t inode_bg, inode_idx;

    ext2_get_inode_idx(&fs->sb, inode_num, &inode_bg, &inode_idx);

    ASSERT(inode_bg < (fs->sb.blocks_count / fs->sb.blocks_per_group));
    ASSERT(inode_idx < fs->sb.inodes_per_group);

    if (!fs->inodes[inode_bg].valid) {
        ext2_populate_inode_cache(fs, inode_bg);
    }

    ASSERT(fs->inodes[inode_bg].valid);
    ASSERT(fs->inodes[inode_bg].inodes != NULL);

    ext2_inode_t* inode = fs->inodes[inode_bg].inodes + (fs->sb_ext.inode_size * inode_idx);
    *inode = *inode_data;

    uint64_t num_inode_blocks = (fs->sb.inodes_per_group * fs->sb_ext.inode_size) / BLOCK_SIZE(fs->sb);

    ext2_write_blocks(fs, fs->bgs[inode_bg].inode_table, num_inode_blocks, fs->inodes[inode_bg].inodes);
}

void ext2_flush_fs(ext2_fs_ctx_t* fs) {

    if (fs->bgs_dirty) {

        uint32_t bg_block;
        if (BLOCK_SIZE(fs->sb) == 1024) {
            bg_block = 2;
        } else {
            bg_block = 1;
        }

        uint32_t num_bgs = fs->sb.blocks_count / fs->sb.blocks_per_group;
        uint32_t num_bgdisk_blocks = ((num_bgs * sizeof(ext2_blockgroup_t)) + BLOCK_SIZE(fs->sb) - 1) / BLOCK_SIZE(fs->sb);

        ext2_write_blocks(fs, bg_block, num_bgdisk_blocks, fs->bgs);

        fs->bgs_dirty = false;
    }

    if (fs->sb_dirty) {
        
        int64_t res;
        uint64_t seek_args[2] = {1024, 0};
        res = fs->disk_ops.ioctl(fs->disk_ctx, IOCTL_SEEK, seek_args, 2);
        ASSERT(res >= 0)

        uint8_t* sb_buffer = vmalloc(1024);
        res = fs->disk_ops.read(fs->disk_ctx, sb_buffer, 1024, 0);
        ASSERT(res >= 0)

        memcpy(sb_buffer, &fs->sb, sizeof(ext2_superblock_t));

        res = fs->disk_ops.ioctl(fs->disk_ctx, IOCTL_SEEK, seek_args, 2);
        ASSERT(res >= 0)

        res = fs->disk_ops.write(fs->disk_ctx, sb_buffer, 1024, 0);
        ASSERT(res >= 0);

        fs->sb_dirty = false;
    }
}

bool ext2_path_is_filename(const char* file) {
    while (*file) {
        if (*file == '/') {
            return false;
        }
        file++;
    }

    return true;
}

const char* ext2_get_filename(const char* file) {
    do {
        if (ext2_path_is_filename(file)) {
            return file;
        }

        while (*file != '/' && *file != '\0') {
            file++;
        }
        if (*file == '/') {
            file++;
        }
    } while (*file);

    ASSERT(0);

    return NULL;
}

ext2_inode_t* ext2_get_parent_dir_inode(ext2_fs_ctx_t* fs, const char* file) {

    uint32_t new_inode_num;
    if (ext2_path_is_filename(file)) {
        new_inode_num = EXT2_ROOT_INODE_NUM;
    } else {

        uint64_t file_len = strlen(file);
        char* file_copy = vmalloc(file_len + 1);
        memset(file_copy, 0, file_len + 1);
        memcpy(file_copy, file, file_len);

        int64_t idx;
        for (idx = file_len; idx >= 0; idx--) {
            if (file_copy[idx] == '/') {
                file_copy[idx] = '\0';
                break;
            }
        }

        ASSERT(idx != 0);

        new_inode_num = ext2_get_inode_for_path(fs, file_copy);

        ASSERT(new_inode_num > 0);
    }

    ext2_inode_t* new_inode = vmalloc(sizeof(ext2_inode_t));
    ext2_get_inode(fs, new_inode_num, new_inode);

    return new_inode;
}

uint32_t ext2_alloc_inode(ext2_fs_ctx_t* fs) {

    uint32_t block_size = BLOCK_SIZE(fs->sb);
    uint32_t num_bgs = fs->sb.blocks_count / fs->sb.blocks_per_group;

    // Loop through each block group
    uint64_t bg_idx = 0;
    for (bg_idx = 0; bg_idx < num_bgs; bg_idx++) {
        if (fs->bgs[bg_idx].free_inodes_count > 0) {
            break;
        }
    }

    // Return 0 on a full disk
    if (bg_idx >= num_bgs) {
        return 0;
    }

    uint32_t num_bitmap_blocks = fs->sb.blocks_per_group;
    uint8_t* b_bitmap_data = vmalloc(block_size);
    int64_t new_inode = -1;

    for (uint64_t idx = 0; idx < num_bitmap_blocks; idx++) {

        ext2_read_block(fs, fs->bgs[bg_idx].inode_bitmap + idx, b_bitmap_data);

        new_inode = ext2_alloc_block_in_bitmap(b_bitmap_data, block_size);

        if (new_inode >= 0) {
            // TODO: This is wrong for other block groups
            // Inode numbers start from 1
            new_inode += idx*(8*block_size) + 1;

            ext2_write_block(fs, fs->bgs[bg_idx].inode_bitmap + idx, b_bitmap_data);
            break;
        }
    }

    ASSERT(new_inode > 0);
    ASSERT(new_inode <= UINT32_MAX);

    fs->bgs[bg_idx].free_inodes_count--;
    fs->sb.free_inodes_count--;
    fs->bgs_dirty = true;
    fs->sb_dirty = true;

    return (uint32_t)new_inode;
}

void ext2_add_file_to_directory(ext2_fs_ctx_t* fs, ext2_inode_t* dir_inode, uint32_t new_inode_num, uint8_t new_inode_type, const char* filename) {

    const uint32_t block_size = BLOCK_SIZE(fs->sb);
    uint8_t* block_buffer = vmalloc(block_size);

    uint32_t block_num = 0;

    uint64_t new_rec_len_min = strlen(filename) + sizeof(ext2_dir_entry_t);
    uint64_t new_rec_len = ((new_rec_len_min + 3) / 4) * 4;

    // TODO: Only check one block for files
    uint32_t dir_block_num = ext2_get_inode_block_num(fs, dir_inode, block_num);
    ext2_read_block(fs, dir_block_num, block_buffer);

    ext2_dir_entry_t* entry;
    uint64_t block_idx = 0;
    do {
        entry = (ext2_dir_entry_t*)&block_buffer[block_idx];

        uint32_t entry_space = (entry->rec_len - (entry->name_len + sizeof(ext2_dir_entry_t))) & (~(3));

        if (entry_space > new_rec_len) {
            break;
        }

        block_idx += entry->rec_len;
    } while (block_idx < block_size);


    // Don't support creating a new block for a record yet.
    ASSERT(block_idx < block_size);

    uint32_t curr_entry_len = ((entry->name_len + sizeof(ext2_dir_entry_t) + 3) / 4) * 4;
    uint32_t curr_entry_padding = entry->rec_len - curr_entry_len;
    entry->rec_len = curr_entry_len;

    block_idx += entry->rec_len;

    memset(&block_buffer[block_idx], 0, new_rec_len);

    entry = (ext2_dir_entry_t *)&block_buffer[block_idx];

    entry->inode = new_inode_num;
    entry->rec_len = curr_entry_padding;
    entry->name_len = strlen(filename);
    entry->file_type = new_inode_type;
    strncpy(entry->name, filename, strlen(filename));

    ext2_write_block(fs, dir_block_num, block_buffer);

    dir_inode->size += new_rec_len;

    vfree(block_buffer);

}

uint32_t ext2_create_inode_at_path(ext2_fs_ctx_t* fs, const char* file) {

    uint32_t block_size = BLOCK_SIZE(fs->sb);

    ext2_inode_t* parent_inode = ext2_get_parent_dir_inode(fs, file);

    uint32_t new_inode_num = ext2_alloc_inode(fs);
    ASSERT(new_inode_num > 0);

    console_printf("Creating new inode (%d) at (%s)\n", new_inode_num, file);

    ext2_inode_t new_inode = {
        .mode = EXT2_S_IFREG | 0777,
        .uid = parent_inode->uid,
        .size = 0,
        .atime = parent_inode->atime,
        .ctime = parent_inode->ctime,
        .mtime = parent_inode->mtime,
        .dtime = parent_inode->dtime,
        .gid = parent_inode->gid,
        .links_count = 1,
        .blocks = block_size / 512,
        .flags = 0,
        .osd1 = 0,
        .block_direct = {0},
        .block_1indirect = 0,
        .block_2indirect = 0,
        .block_3indirect = 0,
        .generation = 0,
        .file_acl = 0,
        .dir_acl = 0,
        .faddr = 0,
        .osd2 = {0}
    };


    uint32_t first_block_num = ext2_alloc_block(fs);
    ASSERT(first_block_num > 0);
    new_inode.block_direct[0] = first_block_num;

    uint8_t* first_block_ptr = vmalloc(block_size);
    memset(first_block_ptr, 0, block_size);

    ext2_write_block(fs, first_block_num, first_block_ptr);

    vfree(first_block_ptr);

    ext2_flush_inode(fs, new_inode_num, &new_inode);

    const char* filename = ext2_get_filename(file);

    ext2_add_file_to_directory(fs, parent_inode, new_inode_num, 0, filename);

    ext2_flush_fs(fs);

    return new_inode_num;
}
