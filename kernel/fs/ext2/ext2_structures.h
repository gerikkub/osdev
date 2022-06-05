
#ifndef __EXT2_STRUCTURES_H__
#define __EXT2_STRUCTURES_H__

// Superblock Structures

typedef struct __attribute__((__packed__)) {
    uint32_t inodes_count;
    uint32_t blocks_count;
    uint32_t r_blocks_count;
    uint32_t free_blocks_count;
    uint32_t free_inodes_count;
    uint32_t first_data_block;
    uint32_t log_block_size;
    uint32_t log_frag_size;
    uint32_t blocks_per_group;
    uint32_t frags_per_group;
    uint32_t inodes_per_group;
    uint32_t mtime;
    uint32_t wtime;
    uint16_t mnt_count;
    uint16_t max_mnt_count;
    uint16_t magic;
    uint16_t state;
    uint16_t errors;
    uint16_t minor_rev_level;
    uint32_t lastcheck;
    uint32_t checkinterval;
    uint32_t creator_os;
    uint32_t rev_level;
    uint16_t def_resuid;
    uint16_t def_resgid;
} ext2_superblock_t;

typedef struct __attribute__((__packed__)) {
    uint32_t first_ino;
    uint16_t inode_size;
    uint16_t block_group_nr;
    uint32_t feature_compat;
    uint32_t feature_incompat;
    uint32_t feature_ro_compat;
    uint8_t uuid[16];
    uint8_t volume_name[16];
    uint8_t last_mounted[64];
    uint32_t algo_bitmap;
} ext2_superblock_extended_t;

typedef struct __attribute__((__packed__)) {
    uint8_t prealloc_blocks;
    uint8_t prealloc_dir_blocks;
    uint16_t res;
} ext2_superblock_performance_t;

typedef struct __attribute__((__packed__)) {
    uint8_t journal_uuid[16];
    uint32_t journal_inum;
    uint32_t journal_dev;
    uint32_t last_orphan;
} ext2_superblock_journal_t;

typedef struct __attribute__((__packed__)) {
    uint32_t hash_seed[4];
    uint8_t def_hash_version;
    uint8_t res[3];
} ext2_superblock_indexing_t;

typedef struct __attribute__((__packed__)) {
    uint32_t default_mount_options;
    uint32_t first_meta_bg;
} ext2_superblock_other_t;

enum {
    EXT2_ERRORS_CONTINUE = 1,
    EXT2_ERRORS_RO = 2,
    EXT2_ERRORS_PANIC = 3,
};

enum {
    EXT2_OS_LINUX = 0,
    EXT2_OS_HURD = 1,
    EXT2_OS_MASIX = 2,
    EXT2_OS_FREEBSD = 3,
    EXT2_OS_LITES = 4
};

enum {
    EXT2_GOOD_OLD_REV = 0,
    EXT2_DYNAMIC_REV = 1
};

enum {
    EXT2_FEATURE_COMPAT_DIR_PREALLOC = 0x1,
    EXT2_FEATURE_COMPAT_IMAGIC_INODES = 0x2,
    EXT3_FEATURE_COMPAT_HAS_JOURNAL = 0x4,
    EXT2_FEATURE_COMPAT_EXT_ATTR = 0x8,
    EXT2_FEATURE_COMPAT_RESIZE_INO = 0x10,
    EXT2_FEATURE_COMPAT_DIR_INDEX = 0x20
};

enum {
    EXT2_FEATURE_INCOMPAT_COMPRESSION = 0x1,
    EXT2_FEATURE_INCOMPAT_FILETYPE = 0x2,
    EXT3_FEATURE_INCOMPAT_RECOVER = 0x4,
    EXT3_FEATURE_INCOMPAT_JOURNAL_DEV = 0x8,
    EXT2_FEATURE_INCOMPAT_META_BG = 0x10
};

enum {
    EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER = 0x1,
    EXT2_FEATURE_RO_COMPAT_LARGE_FILE = 0x2,
    EXT2_FEATURE_RO_COMPAT_BTREE_DIR = 0x4
};

enum {
    EXT2_LZV1_ALG = 0x1,
    EXT2_LZRW3A_ALG = 0x2,
    EXT2_GZIP_ALG = 0x4,
    EXT2_BZIP2_ALG = 0x8,
    EXT2_LZO_ALG = 0x10,
};

// Block Group Desciptor Structures

typedef struct __attribute__((__packed__)) {
    uint32_t block_bitmap;
    uint32_t inode_bitmap;
    uint32_t inode_table;
    uint16_t free_blocks_count;
    uint16_t free_inodes_count;
    uint16_t used_dirs_count;
    uint16_t pad;
    uint8_t res[12];
} ext2_blockgroup_t;

// Inode Structures

typedef struct __attribute__((__packed__)) {
    uint16_t mode;
    uint16_t uid;
    uint32_t size;
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
    uint32_t dtime;
    uint16_t gid;
    uint16_t links_count;
    uint32_t blocks;
    uint32_t flags;
    uint32_t osd1;
    uint32_t block_direct[12];
    uint32_t block_1indirect;
    uint32_t block_2indirect;
    uint32_t block_3indirect;
    uint32_t generation;
    uint32_t file_acl;
    uint32_t dir_acl;
    uint32_t faddr;
    uint8_t osd2[12];
} ext2_inode_t;

enum {
    EXT2_BAD_INO = 1,
    EXT2_ROOT_INO = 2,
    EXT2_ACL_IDX_INO = 3,
    EXT2_ACL_DATA_INO = 4,
    EXT2_BOOT_LOADER_INO = 5,
    EXT2_UNDEL_DIR_INO = 6,
};

enum {
    // File Format
    EXT2_S_IFSOCK = 0xC000,
    EXT2_S_IFLNK = 0xA000,
    EXT2_S_IFREG = 0x8000,
    EXT2_S_IFBLK = 0x6000,
    EXT2_S_IFDIR = 0x4000,
    EXT2_S_IFCHR = 0x2000,
    EXT2_S_IFIFO = 0x1000,

    // Process Execution User/Group Override
    EXT2_S_ISUID = 0x0800,
    EXT2_S_ISGID = 0x0400,
    EXT2_S_ISVTX = 0x0200,

    // Access Rights
    EXT2_S_IRUSR = 0x0100,
    EXT2_S_IWUSR = 0x0080,
    EXT2_S_IXUSR = 0x0040,
    EXT2_S_IRGRP = 0x0020,
    EXT2_S_IWGRP = 0x0010,
    EXT2_S_IXGRP = 0x0008,
    EXT2_S_IROTH = 0x0004,
    EXT2_S_IWOTH = 0x0002,
    EXT2_S_IXOTH = 0x0001,
};

enum {
    EXT2_SECRM_FL = 0x0001,
    EXT2_UNRM_FL = 0x0002,
    EXT2_COMPR_FL = 0x0004,
    EXT2_SYNC_FL = 0x0008,
    EXT2_IMMUTABLE_FL = 0x0010,
    EXT2_APPEND_FL = 0x0020,
    EXT2_NODUMP_FL = 0x0040,
    EXT2_NOATIME_FL = 0x0080,

    EXT2_DIRTY_FL = 0x0100,
    EXT2_COMPRBLK_FL = 0x0200,
    EXT2_NOCOMPR_FL = 0x0400,
    EXT2_ECOMPR_FL = 0x0800,

    EXT2_BTREE_FL = 0x1000,
    EXT2_INDEX_FL = 0x1000,
    EXT2_IMAGIC_FL = 0x2000,
    EXT2_JOURNAL_DATA_FL = 0x4000,
    EXT2_RESERVED_FL = 0x80000000,
};

// Linked List Directory
typedef struct __attribute__((__packed__)) {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t name_len;
    uint8_t file_type;
    char name[];
} ext2_dir_entry_t;

enum {
    EXT2_ROOT_INODE_NUM = 2
};

#endif


