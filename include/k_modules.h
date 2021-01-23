
#ifndef __K_MODULES_H__
#define __K_MODULES_H__

#include <stdint.h>
#include <stdbool.h>

#define MOD_SUBCLASS_STRLEN 16
#define MAX_MODULE_COMPAT_LEN 32

typedef enum {
    MOD_CLASS_INVALID = 0,
    MOD_CLASS_VFS = 1,
    MOD_CLASS_FS = 2,
    MOD_CLASS_DISCOVERY = 3,
    MOD_CLASS_BLOCKDEV = 4
} module_class_t;

typedef enum {
    MOD_GENERIC_INFO = 1,
    MOD_GENERIC_GETINFO = 2,
    MOD_GENERIC_IOCTL = 3,
    MOD_GENERIC_CTX = 4,
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

typedef enum {
    MOD_BLOCKDEV_READ = 0x1000,
    MOD_BLOCKDEV_WRITE = 0x1001,
    MOD_BLOCKDEV_FLUSH = 0x1002
} module_blockdev_ports_t;

typedef enum {
    MOD_SUBCLASS_VALID_SUBCLASS = 1,
    MOD_SUBCLASS_VALID_COMPAT = 2,
    MOD_SUBCLASS_VALID_PCI = 4
} module_subclass_valid_t;

typedef struct {
    module_subclass_valid_t valid;
    module_class_t class;
    char subclass_name[MOD_SUBCLASS_STRLEN];
    char compat[MAX_MODULE_COMPAT_LEN];
    uint16_t pci_vendor;
    uint16_t pci_device;
} module_subclass_t;

typedef enum {
    MOD_STARTTYPE_ONE = 1,       // Start one instance per startmod call
    MOD_STARTTYPE_MANY = 2,      // A single instance may handle multiple startmod calls
} module_starttype_t;

typedef enum {
    MOD_STARTSEL_CLASS = 1,  // Start a module, specifying the class and subclass
    MOD_STARTSEL_COMPAT = 2, // Start a module, specifying a DTB compatiblity string
    MOD_STARTSEL_PCI = 3,    // Start a module, specifying a PCI vendor device pair
} module_startsel_t;

typedef struct {
    module_startsel_t startsel;

    union {
        module_subclass_t class_subclass;
        struct {
            char compat_name[MAX_MODULE_COMPAT_LEN];
        } compat;
        uint32_t pci_vendor_device;
    } data;
} module_startmod_t;

typedef struct {
    uintptr_t header_phy;

    uintptr_t io_base;
    uint64_t io_size;
    uintptr_t m32_base;
    uint64_t m32_size;
    uintptr_t m64_base;
    uint64_t  m64_size;

    struct {
        bool allocated;
        uint64_t space;
        uintptr_t phy;
        uint64_t len;
    } bar[6];
} module_pci_ctx_t;

typedef struct {
    module_startsel_t startsel;
} module_ctx_t;

#endif