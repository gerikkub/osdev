
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "system/lib/system_lib.h"
#include "system/lib/system_msg.h"
#include "system/lib/system_console.h"
#include "system/lib/system_file.h"
#include "system/lib/system_malloc.h"

#include "include/k_sysfs_struct.h"
#include "include/k_ioctl_common.h"

#include "stdlib/printf.h"

int64_t main(uint64_t tid, char** argv) {

    if (argv[0] == NULL) {
        console_printf("Invalid arguments\n");
        return -1;
    }

    uint64_t target_tid = strtoll(argv[0], NULL, 10);

    int64_t target_fd = system_taskctrl(target_tid);

    if (target_fd < 0) {
        console_printf("Unable to open task\n");
        return -1;
    }

    console_printf("Waiting on %d\n", target_tid);
    console_flush();

    int64_t target_ret = system_ioctl(target_fd, TASKCTRL_IOCTL_WAIT, NULL, 0);

    console_printf("Task %d returned %d\n", target_tid, target_ret);
    console_flush();

    return 0;
}