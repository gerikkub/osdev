
#include <stdint.h>

#include "kernel/assert.h"
#include "kernel/console.h"
#include "kernel/lib/vmalloc.h"
#include "kernel/drivers.h"
#include "kernel/fd.h"
#include "kernel/task.h"
#include "kernel/sys_device.h"

#include "stdlib/bitutils.h"
#include "stdlib/string.h"

static int64_t console_dev_open(void* ctx, const char* path, const uint64_t flags, void** ctx_out) {
    *ctx_out = NULL;
    return 0;
}

static int64_t console_dev_write(void* ctx, const uint8_t* buffer, const int64_t size, const uint64_t flags) {

    task_t* task = get_active_task();

    console_printf("[%s] %s", task->name, buffer);

    return size;
}

static fd_ops_t s_console_ops = {
    .read = NULL,
    .write = console_dev_write,
    .seek = NULL,
    .ioctl = NULL,
    .close = NULL
};

void console_register(void) {
    sys_device_register(&s_console_ops, console_dev_open, NULL, "con");
}

REGISTER_DRIVER(console);