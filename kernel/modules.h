
#ifndef __MODULES_H__
#define __MODULES_H__

void modules_init_list(void);

void modules_start(void);

typedef enum {
    MOD_CLASS_INVALID = 0,
    MOD_CLASS_VFS = 1,
    MOD_CLASS_FS = 2,
} module_class_t;

typedef enum {
    MOD_GENERIC_INFO = 1,
    MOD_GENERIC_GETINFO = 2,
    MOD_GENERIC_IOCTL = 3
} module_generic_ports_t;

typedef enum {
    MOD_VFS_INFO = 1,
    MOD_VFS_GETINFO = 2,
    MOD_VFS_IOCTL = 3
} module_vfs_ports_t;

typedef enum {
    MOD_FS_INFO = 1,
    MOD_FS_GETINFO = 2,
    MOD_FS_IOCTL = 3
} module_fs_ports_t;

int64_t syscall_getmod(uint64_t class,
                       uint64_t subclass0,
                       uint64_t subclass1,
                       uint64_t x3);

#endif