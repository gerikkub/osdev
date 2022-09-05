
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

int64_t sysfs_time_close(void* ctx) {

    vfree(ctx);

    return 0;
}

void* sysfs_time_open(void) {

    llist_head_t data_list = llist_create();

    char* data_str = vmalloc(4096);
    uint64_t curr_time = gtimer_get_count();
    uint64_t freq = gtimer_get_frequency();
    uint64_t time_s = curr_time / freq;
    uint64_t time_us = ((curr_time - (time_s * freq)) * 1000000) / freq;
    uint64_t len = snprintf(data_str, 4096, "%u %u", time_s, time_us);

    file_data_entry_t* data_entry = vmalloc(sizeof(file_data_entry_t));
    data_entry->data = (void*)data_str;
    data_entry->len = len;
    data_entry->dirty = 0;
    data_entry->available = 1;

    llist_append_ptr(data_list, data_entry);

    file_ctx_t file_ctx_in = {
        .file_data = data_list,
        .size = len,
        .seek_idx = 0,
        .can_write = false,
        .close_op = sysfs_time_close,
        .populate_op = NULL,
        .flush_data_op = NULL,
        .op_ctx = data_str
    };

    void* file_ctx = file_create_ctx(&file_ctx_in);

    return file_ctx;
}

int64_t sysfs_time_raw_close(void* ctx) {

    vfree(ctx);

    return 0;
}

void* sysfs_time_raw_open(void) {

    llist_head_t data_list = llist_create();

    ksysfs_struct_time_raw_t* data_ptr = vmalloc(sizeof(ksysfs_struct_time_raw_t));
    uint64_t curr_time = gtimer_get_count();
    uint64_t freq = gtimer_get_frequency();
    uint64_t time_s = curr_time / freq;
    uint64_t time_us = ((curr_time - (time_s * freq)) * 1000000) / freq;

    data_ptr->time_s = time_s;
    data_ptr->time_us = time_us;

    file_data_entry_t* data_entry = vmalloc(sizeof(file_data_entry_t));
    data_entry->data = (void*)data_ptr;
    data_entry->len = sizeof(ksysfs_struct_time_raw_t);
    data_entry->dirty = 0;
    data_entry->available = 1;

    llist_append_ptr(data_list, data_entry);

    file_ctx_t file_ctx_in = {
        .file_data = data_list,
        .size = sizeof(ksysfs_struct_time_raw_t),
        .seek_idx = 0,
        .can_write = false,
        .close_op = sysfs_time_raw_close,
        .populate_op = NULL,
        .flush_data_op = NULL,
        .op_ctx = data_ptr
    };

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