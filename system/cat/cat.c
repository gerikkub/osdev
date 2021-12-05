
#include <stdint.h>
#include <string.h>

#include "system/lib/system_lib.h"
#include "system/lib/system_msg.h"
#include "system/lib/system_console.h"
#include "system/lib/system_assert.h"
#include "system/lib/system_file.h"
#include "system/lib/system_malloc.h"

#include "include/k_syscall.h"
#include "include/k_messages.h"
#include "include/k_modules.h"
#include "include/k_ioctl_common.h"

#include "stdlib/printf.h"

int64_t main(uint64_t tid, char** ctx) {

    int64_t fd;
    fd = system_open(ctx[0], ctx[1], 0);
    SYS_ASSERT(fd >= 0);

    int64_t len;
    len = system_ioctl(fd, BLK_IOCTL_SIZE, NULL, 0);
    SYS_ASSERT(len > 0);

    char* buffer = malloc(len);
    int64_t read_len;
    read_len = system_read(fd, buffer, len, 0);
    SYS_ASSERT(read_len > 0);

    console_write_len(buffer, read_len);
    console_flush();

    free(buffer);

    return 0;
}