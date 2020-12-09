
#ifndef __K_MODULES_H__
#define __K_MODULES_H__

#define MOD_SUBCLASS_STRLEN 16
#define MAX_MODULE_COMPAT_LEN 32

typedef enum {
    MOD_CLASS_INVALID = 0,
    MOD_CLASS_VFS = 1,
    MOD_CLASS_FS = 2,
    MOD_CLASS_DISCOVERY = 3,
} module_class_t;

typedef enum {
    MOD_GENERIC_INFO = 1,
    MOD_GENERIC_GETINFO = 2,
    MOD_GENERIC_IOCTL = 3,
    MOD_GENERIC_DTB = 4,
    MAX_MOD_GENERIC_PORT = 0x1000
} module_generic_ports_t;

typedef enum {
    MOD_VFS_DUMMY = 0x1000
} module_vfs_ports_t;

typedef enum {
    MOD_FS_STR = 0x1000
} module_fs_ports_t;

typedef enum {
    MOD_DISCOVERY_DUMMY = 0x1000
} module_discovery_ports_t;

typedef struct {
    module_class_t class;
    char subclass_name[MOD_SUBCLASS_STRLEN];
} module_subclass_t;

typedef enum {
    MOD_STARTTYPE_ONE = 1,       // Start one instance per startmod call
    MOD_STARTTYPE_MANY = 2,      // A single instance may handle multiple startmod calls
} module_starttype_t;

typedef enum {
    MOD_STARTSEL_CLASS = 1,  // Start a module, specifying the class and subclass
    MOD_STARTSEL_COMPAT = 2, // Start a module, specifying a DTB compatiblity string
} module_startsel_t;

typedef struct {
    module_startsel_t startsel;

    union {
        module_subclass_t class_subclass;
        struct {
            char compat_name[MAX_MODULE_COMPAT_LEN];
        } compat;
    } data;
} module_startmod_t;

typedef struct {
    module_startsel_t startsel;
} module_ctx_t;

#endif