
#include <stdint.h>

#include "kernel/console.h"
#include "kernel/elf.h"
#include "kernel/vfs.h"
#include "kernel/fd.h"
#include "kernel/assert.h"
#include "kernel/task.h"
#include "kernel/lib/vmalloc.h"

#include "include/k_ioctl_common.h"

uint64_t exec_user_task(const char* device, const char* path, const char* name, char** argv) {

    int64_t file_fd;
    fd_ctx_t* file_fd_ctx;

    file_fd = vfs_open_device_fd(device, path, 0);
    if (file_fd < 0) {
        console_log(LOG_INFO, "Cannot Exec: Can't open file %d", file_fd);
        return 0;
    }
    file_fd_ctx = get_kernel_fd(file_fd);
    ASSERT(file_fd_ctx != NULL);

    int64_t file_size = fd_call_ioctl(file_fd_ctx, BLK_IOCTL_SIZE, NULL, 0);
    if (file_size < 0) {
        console_log(LOG_INFO, "Cannot Exec: Can't get file size %d", file_size);
        fd_call_close(file_fd_ctx);
        return 0;
    }

    uint8_t* read_buffer = vmalloc(file_size);

    int64_t size = fd_call_read(file_fd_ctx, read_buffer, file_size, 0);
    if(size != file_size) {
        console_log(LOG_INFO, "Cannot Exec: Can't read file %d (%d)", size, file_size);
        vfree(read_buffer);
        fd_call_close(file_fd_ctx);
        return 0;
    }

    uint64_t tid = 0;
    elf_result_t res;
    tid = create_elf_task(read_buffer, size, &res, false, name, argv);
    if (tid == 0) {
        console_log(LOG_INFO, "Cannot Exec: ELF load failed %d", res);
    }

    vfree(read_buffer);
    fd_call_close(file_fd_ctx);

    return res == ELF_VALID ? tid : 0;
}
