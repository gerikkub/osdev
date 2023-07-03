
#include <stdint.h>
#include <string.h>

#include "kernel/assert.h"
#include "kernel/console.h"
#include "kernel/vfs.h"
#include "kernel/panic.h"

#include "stdlib/bitutils.h"

#define MAX_NUM_DEVICES 64

static vfs_device_ops_t s_devices[MAX_NUM_DEVICES] = {0};

void vfs_register_device(vfs_device_ops_t* device) {

    ASSERT(device != NULL);

    ASSERT(device->name[MAX_DEVICE_NAME_LEN-1] == 0);

    console_log(LOG_INFO, "Registering %s FS", device->name);

    int idx;
    for (idx = 0; idx < MAX_NUM_DEVICES; idx++) {
        if (!s_devices[idx].valid) {
            s_devices[idx] = *device;
            s_devices[idx].valid = true;
            break;
        }
    }

    if (idx >= MAX_NUM_DEVICES) {
        PANIC("Unable to register %s FS", device->name);
    }
}

int64_t vfs_open_device(const char* device_name, const char* path, uint64_t flags, fd_ops_t* ops_out, void** ctx_out, fd_ctx_t* fd_ctx) {
    ASSERT(device_name != NULL);
    ASSERT(path != NULL);
    ASSERT(ops_out != NULL);
    ASSERT(ctx_out != NULL);

    for (int idx = 0; idx < MAX_NUM_DEVICES; idx++) {
        if (s_devices[idx].valid &&
            strncmp(s_devices[idx].name, device_name, MAX_DEVICE_NAME_LEN) == 0) {

            int64_t res = s_devices[idx].open(s_devices[idx].ctx, path, flags, ctx_out, fd_ctx);
            if (res >= 0) {
                *ops_out = s_devices[idx].fd_ops;
            }
            return res;
        }
    }

    return -1;
}
