
#include <stdint.h>

#include <kernel/assert.h>
#include <kernel/fs/sysfs/sysfs.h>
#include <kernel/fs/file.h>
#include <kernel/fd.h>
#include <kernel/task.h>
#include <kernel/lib/vmalloc.h>

#include <stdlib/string.h>
#include <stdlib/bitutils.h>

typedef struct {
    llist_head_t data_list;
} sysfs_task_ctx_t;

int64_t sysfs_task_close(void*);

void* sysfs_task_open(void) {

    sysfs_task_ctx_t* free_ctx = vmalloc(sizeof(sysfs_task_ctx_t));
    free_ctx->data_list = llist_create();

    uint64_t task_size = 0;
    uint64_t task_idx = 0;
    while (task_idx < MAX_NUM_TASKS) {
        task_t* task = get_task_at_idx(task_idx);
        if (task != NULL) {

            uint8_t* data_str = vmalloc(4096);
            int64_t written = snprintf((char*)data_str, 4096,
                "%d %s:\n"
                "  name: %s\n"
                "  state: %s\n"
                ,
                task->tid & (~TASK_TID_KERNEL),
                IS_USER_TASK(task->tid) ? "user" : "kernel",
                task->name,
                task->run_state == TASK_RUNABLE ? "RUNABLE" :
                task->run_state == TASK_RUNABLE_KERNEL ? "RUNABLE_KERNEL" :
                task->run_state == TASK_WAIT ? "WAIT" :
                task->run_state == TASK_AWAKE ? "AWAKE" : "INVALID"
            );
            ASSERT(written < 4096);
            file_data_entry_t* data_entry = vmalloc(sizeof(file_data_entry_t));
            data_entry->data = data_str;
            data_entry->len = written;
            data_entry->dirty = 0;
            data_entry->available = 1;

            llist_append_ptr(free_ctx->data_list, data_entry);

            task_size += written;
        }

        task_idx++;
    }

    file_ctx_t file_ctx_in = {
        .file_data = free_ctx->data_list,
        .size = task_size,
        .seek_idx = 0,
        .can_write = false,
        .close_op = sysfs_task_close,
        .populate_op = NULL,
        .flush_data_op = NULL,
        .op_ctx = free_ctx,
    };

    void* file_ctx = file_create_ctx(&file_ctx_in);

    return file_ctx;
}

int64_t sysfs_task_close(void* ctx) {

    sysfs_task_ctx_t* task_ctx = ctx;
    file_data_entry_t* entry;

    FOR_LLIST(task_ctx->data_list, entry)
        vfree(entry->data);
        vfree(entry);
    END_FOR_LLIST()

    llist_free(task_ctx->data_list);

    return 0;
}


void sysfs_task_init(void) {

    fd_ops_t ops = {
        .read = file_read_op,
        .write = file_write_op,
        .ioctl = file_ioctl_op,
        .close = file_close_op
    };

    sysfs_create_file("tasks", sysfs_task_open, &ops);
}