
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "kernel/syscall.h"
#include "kernel/exception.h"
#include "kernel/task.h"
#include "kernel/assert.h"
#include "kernel/fd.h"
#include "kernel/vmem.h"
#include "kernel/vfs.h"
#include "kernel/kernelspace.h"

int64_t syscall_open(uint64_t device, uint64_t path, uint64_t flags, uint64_t dummy) {

    task_t* task = get_active_task();

    uint64_t device_phy;
    bool walk_ok;
    walk_ok = vmem_walk_table(task->low_vm_table, device, &device_phy);
    if (!walk_ok) {
        return -1;
    }
    void* device_kptr = PHY_TO_KSPACE_PTR(device_phy);

    uint64_t path_phy;
    walk_ok = vmem_walk_table(task->low_vm_table, path, &path_phy);
    if (!walk_ok) {
        return -1;
    }
    void* path_kptr = PHY_TO_KSPACE_PTR(path_phy);

    return vfs_open_device_fd(device_kptr, path_kptr, flags);
}

int64_t syscall_read(uint64_t fd, uint64_t buffer, uint64_t len, uint64_t flags) {

    task_t* task = get_active_task();

    if (fd >= MAX_TASK_FDS) {
        return -1;
    }

    if (!task->fds[fd].valid &&
        task->fds[fd].ops.read != NULL) {
        return -1;
    }

    uint64_t buffer_phy;
    bool walk_ok;
    void* buffer_kptr;
    walk_ok = vmem_walk_table(task->low_vm_table, buffer, &buffer_phy);
    if (!walk_ok) {
        return -1;
    }
    buffer_kptr = PHY_TO_KSPACE_PTR(buffer_phy);

    if (task->fds[fd].ops.read != NULL) {
        return fd_call_read(&task->fds[fd], buffer_kptr, len, flags);
    } else {
        return -1;
    }
}

int64_t syscall_write(uint64_t fd, uint64_t buffer, uint64_t len, uint64_t flags) {

    task_t* task = get_active_task();

    if (fd >= MAX_TASK_FDS) {
        return -1;
    }

    if (!task->fds[fd].valid &&
        task->fds[fd].ops.write != NULL) {
        return -1;
    }

    uint64_t buffer_phy;
    bool walk_ok;
    void* buffer_kptr;
    walk_ok = vmem_walk_table(task->low_vm_table, buffer, &buffer_phy);
    if (!walk_ok) {
        return -1;
    }
    buffer_kptr = PHY_TO_KSPACE_PTR(buffer_phy);

    if (task->fds[fd].ops.write != NULL) {
        return fd_call_write(&task->fds[fd], buffer_kptr, len, flags);
    } else {
        return -1;
    }

}

int64_t syscall_ioctl(uint64_t fd, uint64_t ioctl, uint64_t args, uint64_t arg_count) {
    task_t* task = get_active_task();

    if (fd >= MAX_TASK_FDS) {
        return -1;
    }

    if ((!task->fds[fd].valid) ||
         task->fds[fd].ops.ioctl == NULL) {
        return -1;
    }

    if (task->fds[fd].ops.ioctl == NULL) {
        return -1;
    }

    if (arg_count > 0) {
        uint64_t args_phy;
        bool walk_ok;
        void* args_kptr;
        walk_ok = vmem_walk_table(task->low_vm_table, args, &args_phy);
        if (!walk_ok) {
            return -1;
        }
        args_kptr = PHY_TO_KSPACE_PTR(args_phy);

        return fd_call_ioctl(&task->fds[fd], ioctl, args_kptr, arg_count);
    } else {
        return fd_call_ioctl(&task->fds[fd], ioctl, NULL, 0);
    }
}

int64_t syscall_close(uint64_t fd, uint64_t arg1, uint64_t arg2, uint64_t arg3) {

    task_t* task = get_active_task();

    if (fd >= MAX_TASK_FDS) {
        return -1;
    }

    if (!task->fds[fd].valid &&
        task->fds[fd].ops.close != NULL) {
        return -1;
    }

    if (task->fds[fd].ops.close == NULL) {
        task->fds[fd].valid = false;
        return 0;
    } else {
        int64_t ret = fd_call_close(&task->fds[fd]);
        task->fds[fd].valid = false;
        return ret;
    }
}

int64_t fd_call_read(fd_ctx_t* fd_ctx, uint8_t* buffer, const int64_t size, const uint64_t flags) {
    ASSERT(fd_ctx != NULL);
    ASSERT(fd_ctx->valid);

    return fd_ctx->ops.read(fd_ctx->ctx, buffer, size, flags);
}

int64_t fd_call_write(fd_ctx_t* fd_ctx, const uint8_t* buffer, const int64_t size, const uint64_t flags) {
    ASSERT(fd_ctx != NULL);
    ASSERT(fd_ctx->valid);

    return fd_ctx->ops.write(fd_ctx->ctx, buffer, size, flags);
}

int64_t fd_call_ioctl(fd_ctx_t* fd_ctx, const uint64_t ioctl, const uint64_t *args, const uint64_t arg_count) {
    ASSERT(fd_ctx != NULL);
    ASSERT(fd_ctx->valid);

    return fd_ctx->ops.ioctl(fd_ctx->ctx, ioctl, args, arg_count);
}

int64_t fd_call_ioctl_ptr(fd_ctx_t* fd_ctx, const uint64_t ioctl, void* arg) {
    ASSERT(fd_ctx != NULL);
    ASSERT(fd_ctx->valid);

    uint64_t ioctl_args = (uint64_t)arg;
    return fd_call_ioctl(fd_ctx, ioctl, &ioctl_args, 1);
}

int64_t fd_call_close(fd_ctx_t* fd_ctx) {
    ASSERT(fd_ctx != NULL);
    ASSERT(fd_ctx->valid);

    return fd_ctx->ops.close(fd_ctx->ctx);
}

