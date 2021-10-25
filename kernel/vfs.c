
#include <stdint.h>

#include "kernel/assert.h"
#include "kernel/console.h"
#include "kernel/vfs.h"

#include "stdlib/bitutils.h"
#include "stdlib/string.h"

#define MAX_NUM_DEVICES 64

static vfs_device_ops_t s_devices[MAX_NUM_DEVICES] = {0};

void vfs_register_device(vfs_device_ops_t* device) {

    ASSERT(device != NULL);

    ASSERT(device->name[MAX_DEVICE_NAME_LEN-1] == 0);

    for (int idx = 0; idx < MAX_NUM_DEVICES; idx++) {
        if (!s_devices[idx].valid) {
            s_devices[idx] = *device;
            s_devices[idx].valid = true;
            break;
        }
    }
}

int64_t vfs_open_device(const char* device_name, const char* path, uint64_t flags, fd_ops_t* ops_out, void** ctx_out) {
    ASSERT(device_name != NULL);
    ASSERT(path != NULL);
    ASSERT(ops_out != NULL);
    ASSERT(ctx_out != NULL);

    for (int idx = 0; idx < MAX_NUM_DEVICES; idx++) {
        if (s_devices[idx].valid &&
            strncmp(s_devices[idx].name, device_name, MAX_DEVICE_NAME_LEN) == 0) {

            int64_t res = s_devices[idx].open(s_devices[idx].ctx, path, flags, ctx_out);
            if (res >= 0) {
                *ops_out = s_devices[idx].fd_ops;
            }
            return res;
        }
    }

    return -1;
}
