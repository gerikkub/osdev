
#ifndef __SYS_DEVICE_H__
#define __SYS_DEVICE_H__

#include <stdint.h>

#include "kernel/fd.h"
#include "kernel/vfs.h"

#define MAX_SYS_DEVICE_NAME_LEN 64

void sys_device_init(void);

int64_t sys_device_register(fd_ops_t* ops, device_open_op open_op, void* ctx, const char* name);

#endif
