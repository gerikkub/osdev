
#ifndef __K_MODULES_H__
#define __K_MODULES_H__

typedef enum {
    MOD_CLASS_INVALID = 0,
    MOD_CLASS_VFS = 1,
    MOD_CLASS_FS = 2,
    MOD_CLASS_DISCOVERY = 3,
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
    MOD_FS_IOCTL = 3,
    MOD_FS_STR = 4
} module_fs_ports_t;

typedef enum {
    MOD_DISCOVERY_INFO = 1,
    MOD_DISCOVERY_GETINFO = 2,
    MOD_DISCOVERY_IOCTL = 3
} module_discovery_ports_t;

#endif