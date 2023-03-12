
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
        ctx[1] == NULL ||
        ctx[2] == NULL) {
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

    system_ioctl(fd, IOCTL_SEEK_END, NULL, 0);

    system_write(fd, ctx[2], strlen(ctx[2]), 0);
    system_write(fd, "\n", 1, 0);

    return 0;
}
