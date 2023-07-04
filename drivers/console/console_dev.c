
#include <stdint.h>
#include <string.h>

#include "kernel/assert.h"
#include "kernel/console.h"
#include "kernel/lib/vmalloc.h"
#include "kernel/drivers.h"
#include "kernel/fd.h"
#include "kernel/task.h"
#include "kernel/sys_device.h"

#include "stdlib/bitutils.h"
#include "stdlib/printf.h"

static int64_t console_dev_open(void* ctx, const char* path, const uint64_t flags, void** ctx_out, fd_ctx_t* fd_ctx) {
    *ctx_out = NULL;
    return 0;
}

static int64_t console_dev_read(void* ctx, uint8_t* buffer, const int64_t size, const uint64_t flags) {
    char c = console_getchar();

    buffer[0] = c;
    return 1;
}

static int64_t console_dev_write(void* ctx, const uint8_t* buffer, const int64_t size, const uint64_t flags) {

    // task_t* task = get_active_task();

    // console_printf("[%s] %s", task->name, buffer);
    console_printf("%s", buffer);

    return size;
}

static fd_ops_t s_console_ops = {
    .read = console_dev_read,
    .write = console_dev_write,
    .ioctl = NULL,
    .close = NULL
};

void console_register(void) {
    sys_device_register(&s_console_ops, console_dev_open, NULL, "con");
}

REGISTER_DRIVER(console);