
#include <stdint.h>
#include <string.h>

#include <kernel/assert.h>
#include <kernel/fs/sysfs/sysfs.h>
#include <kernel/fs/file.h>
#include <kernel/fd.h>
#include <kernel/lib/vmalloc.h>
#include <kernel/lib/elapsedtimer.h>
#include <kernel/task.h>
#include <kernel/interrupt/interrupt.h>

#include <include/k_sysfs_struct.h>

#include <stdlib/bitutils.h>
#include <stdlib/printf.h>

void* sysfs_profile_open(void) {

    char* data_str = vmalloc(4096);
    memset(data_str, 0, 4096);
    uint64_t str_idx = 0;

    str_idx += snprintf(&data_str[str_idx], 4096-str_idx,
                        "Schedule: %u\n", get_schedule_profile_time() / (1000));
    for (int64_t idx = 0; idx < 1029; idx++) {
        uint64_t intid;
        uint64_t elapsed = interrupt_get_profile_time(idx, &intid);
        if (elapsed > 0) {
            str_idx += snprintf(&data_str[str_idx], 4096-str_idx,
                                "IRQ %u: %u\n", intid, elapsed / (1000));
        }
    }

    for (int64_t idx = 0; idx < MAX_NUM_TASKS; idx++) {
        task_t* task = get_task_at_idx(idx);
        if (task == NULL) {
            continue;
        }

        if (task->tid != 0) {
            str_idx += snprintf(&data_str[str_idx], 4095 - str_idx,
                                "%s (%u): %u\n",
                                task->name, task->tid,
                                elapsedtimer_get_us(&task->profile_time) / (1000));
        }
    }

    file_ctx_t file_ctx_in;

    sysfs_ro_file_helper(data_str, str_idx, &file_ctx_in);

    void* file_ctx = file_create_ctx(&file_ctx_in);

    return file_ctx;
}

void sysfs_profile_init(void) {

    fd_ops_t ops = {
        .read = file_read_op,
        .write = file_write_op,
        .ioctl = file_ioctl_op,
        .close = file_close_op
    };

    sysfs_create_file("profile", sysfs_profile_open, &ops);
}
