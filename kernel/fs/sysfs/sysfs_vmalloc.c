
#include <stdint.h>

#include <kernel/assert.h>
#include <kernel/fs/sysfs/sysfs.h>
#include <kernel/fs/file.h>
#include <kernel/fd.h>
#include <kernel/lib/vmalloc.h>
#include <kernel/gtimer.h>

#include <stdlib/string.h>
#include <stdlib/bitutils.h>
#include <stdlib/malloc.h>

void* sysfs_vmalloc_stat_open(void) {

    char* data_str = vmalloc(4096);
    uint64_t data_str_len = 0;

    malloc_stat_t vmalloc_stat;
    vmalloc_calc_stat(&vmalloc_stat);

    data_str_len = snprintf(data_str, 4096, "%u %u %u",
                            vmalloc_stat.total_mem,
                            vmalloc_stat.avail_mem,
                            vmalloc_stat.largest_chunk);

    file_ctx_t file_ctx_in;

    sysfs_ro_file_helper(data_str, data_str_len, &file_ctx_in);

    void* file_ctx = file_create_ctx(&file_ctx_in);

    return file_ctx;
}

void sysfs_vmalloc_init(void) {

    fd_ops_t ops = {
        .read = file_read_op,
        .write = file_write_op,
        .ioctl = file_ioctl_op,
        .close = file_close_op
    };

    sysfs_create_file("vmalloc_stat", sysfs_vmalloc_stat_open, &ops);
}