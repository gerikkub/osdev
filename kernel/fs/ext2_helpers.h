
#ifndef __EXT2_HELPERS_H__
#define __EXT2_HELPERS_H__

#include <stdint.h>

#include "kernel/lock/lock.h"

#include "kernel/fs/ext2_structures.h"

#define BLOCK_SIZE(sb) (1024 << (sb.log_block_size))

typedef struct {
    bool valid;
    void* inodes;
} ext2_inode_cache_t;

typedef struct {
    ext2_superblock_t sb;
    ext2_superblock_extended_t sb_ext;
    ext2_superblock_performance_t sb_perf;
    ext2_superblock_journal_t sb_journal;
    ext2_superblock_indexing_t sb_indexing;
    ext2_superblock_other_t sb_other;
    ext2_blockgroup_t* bgs;

    ext2_inode_cache_t* inodes;

    void* disk_ctx;
    fd_ops_t disk_ops;

    lock_t fs_lock;
} ext2_fs_ctx_t;

void ext2_find_inode(ext2_superblock_t* sb, const uint32_t inode,
                     uint32_t* bg_out, uint32_t* ino_idx_out);

void ext2_disk_read(ext2_fs_ctx_t* fs, uint64_t offset, void* buffer, uint64_t size);

void ext2_read_block(ext2_fs_ctx_t* fs, const uint64_t block, void* buffer);

void ext2_read_blocks(ext2_fs_ctx_t* fs,
                      const uint64_t block_start,
                      const uint64_t num_blocks,
                      void* buffer);

void ext2_populate_inode_cache(ext2_fs_ctx_t* fs, const uint32_t bg);

uint64_t ext2_get_inode_size(ext2_fs_ctx_t* fs, const ext2_inode_t* inode);

void ext2_read_inode_block(ext2_fs_ctx_t* fs, const ext2_inode_t* inode,
                           const uint32_t block_num, uint8_t* buffer);

void ext2_read_inode_data(ext2_fs_ctx_t* fs, const ext2_inode_t* inode,
                          const uint32_t block_start, const uint32_t num_blocks,
                          uint8_t* buffer);

void ext2_get_inode(ext2_fs_ctx_t* fs, const uint32_t inode_num, ext2_inode_t* inode_out);

uint32_t ext2_get_inode_in_dir(ext2_fs_ctx_t* fs, const ext2_inode_t* inode, const char* name);

uint32_t ext2_get_inode_for_path_rel(ext2_fs_ctx_t* fs, const ext2_inode_t* inode_start,
                                 const char* path);

uint32_t ext2_get_inode_for_path(ext2_fs_ctx_t* fs, const char* path);

#endif
