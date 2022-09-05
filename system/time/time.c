
#include <stdint.h>
#include <string.h>

#include "system/lib/system_lib.h"
#include "system/lib/system_msg.h"
#include "system/lib/system_console.h"
#include "system/lib/system_file.h"
#include "system/lib/system_malloc.h"

#include "include/k_sysfs_struct.h"

#include "stdlib/printf.h"

int32_t main(uint64_t tid, char** argv) {

    int64_t fd = system_open("sysfs", "time_raw", 0);
    if (fd < 0) {
        return -1;
    }

    ksysfs_struct_time_raw_t* raw_time = malloc(sizeof(ksysfs_struct_time_raw_t));
    int64_t read_len;
    read_len = system_read(fd, raw_time, sizeof(ksysfs_struct_time_raw_t), 0);
    if (read_len != sizeof(ksysfs_struct_time_raw_t)) {
        return -1;
    }

    console_printf("%u %u\n", raw_time->time_s, raw_time->time_us);

    return 0;
}