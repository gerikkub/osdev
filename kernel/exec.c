
#include <stdint.h>

#include "stdlib/bitutils.h"
#include "kernel/elf.h"
#include "kernel/vfs.h"
#include "kernel/fd.h"
#include "kernel/lib/vmalloc.h"

#include "include/k_ioctl_common.h"

uint64_t exec_user_task(const char* device, const char* path, const char* name, char** argv) {

    int64_t open_res;
    fd_ops_t file_ops;
    void* file_ctx;

    open_res = vfs_open_device(device, path, 0, &file_ops, &file_ctx);
    if (open_res < 0) {
        return 0;
    }

    int64_t file_size = file_ops.ioctl(file_ctx, BLK_IOCTL_SIZE, NULL, 0);
    if (file_size < 0) {
        return 0;
    }

    uint8_t* read_buffer = vmalloc(file_size);

    int64_t size = file_ops.read(file_ctx, read_buffer, file_size, 0);
    if(size != file_size) {
        vfree(read_buffer);
        return 0;
    }

    uint64_t tid;
    elf_result_t res;
    tid = create_elf_task(read_buffer, size, &res, false, name, argv);

    vfree(read_buffer);

    return res == ELF_VALID ? tid : 0;
}