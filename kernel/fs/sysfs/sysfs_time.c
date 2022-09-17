
#include <stdint.h>

#include <kernel/assert.h>
#include <kernel/fs/sysfs/sysfs.h>
#include <kernel/fs/file.h>
#include <kernel/fd.h>
#include <kernel/lib/vmalloc.h>
#include <kernel/gtimer.h>

#include <include/k_sysfs_struct.h>

#include <stdlib/string.h>
#include <stdlib/bitutils.h>

void* sysfs_time_open(void) {

    char* data_str = vmalloc(4096);
    uint64_t curr_time = gtimer_get_count();
    uint64_t freq = gtimer_get_frequency();
    uint64_t time_s = curr_time / freq;
    uint64_t time_us = ((curr_time - (time_s * freq)) * 1000000) / freq;
    uint64_t len = snprintf(data_str, 4096, "%u %u", time_s, time_us);

    file_ctx_t file_ctx_in;

    sysfs_ro_file_helper(data_str, len, &file_ctx_in);

    void* file_ctx = file_create_ctx(&file_ctx_in);

    return file_ctx;
}

void* sysfs_time_raw_open(void) {

    ksysfs_struct_time_raw_t* data_ptr = vmalloc(sizeof(ksysfs_struct_time_raw_t));
    uint64_t curr_time = gtimer_get_count();
    uint64_t freq = gtimer_get_frequency();
    uint64_t time_s = curr_time / freq;
    uint64_t time_us = ((curr_time - (time_s * freq)) * 1000000) / freq;

    data_ptr->time_s = time_s;
    data_ptr->time_us = time_us;

    file_ctx_t file_ctx_in;

    sysfs_ro_file_helper(data_ptr, sizeof(ksysfs_struct_time_raw_t), &file_ctx_in);

    void* file_ctx = file_create_ctx(&file_ctx_in);

    return file_ctx;

}


void sysfs_time_init(void) {

    fd_ops_t ops = {
        .read = file_read_op,
        .write = file_write_op,
        .ioctl = file_ioctl_op,
        .close = file_close_op
    };

    sysfs_create_file("time", sysfs_time_open, &ops);
    sysfs_create_file("time_raw", sysfs_time_raw_open, &ops);
}