
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

    if (ctx[0] == NULL ||
        ctx[1] == NULL) {
        console_printf("Invalid Arguments\n");
        console_flush();
        return -1;
    }

    int64_t fd;
    fd = system_open(ctx[0], ctx[1], 0);
    if (fd < 0) {
        console_printf("%s:%s does not exist\n", ctx[0], ctx[1]);
        return -1;
    }
    SYS_ASSERT(fd >= 0);

    // int64_t len;
    // len = system_ioctl(fd, BLK_IOCTL_SIZE, NULL, 0);
    // if (len < 0) {
    //     console_printf("Invalid length\n");
    //     console_flush();
    //     return -1;
    // } else if (len == 0) {
    //     return 0;
    // }

    char* buffer = malloc(4096);
    int64_t read_len;
    do {
        read_len = system_read(fd, buffer, 4096, 0);
        console_write_len(buffer, read_len);
        console_flush();
    } while (read_len > 0);


    free(buffer);

    return 0;
}