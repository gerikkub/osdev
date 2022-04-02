
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

static int64_t find_open_fd(task_t* task) {

    for (int idx = 0; idx < MAX_TASK_FDS; idx++) {
        if (!task->fds[idx].valid) {
            return idx;
        }
    }

    return -1;
}

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

    int64_t ret;
    int64_t fd_num = find_open_fd(task);
    if (fd_num < 0) {
        return -1;
    }

    ret = vfs_open_device(device_kptr, path_kptr, flags, &task->fds[fd_num].ops, &task->fds[fd_num].ctx);
    if (ret >= 0) {
        task->fds[fd_num].valid = true;
        return fd_num;
    } else {
        return -1;
    }

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

    return task->fds[fd].ops.read(task->fds[fd].ctx, buffer_kptr, len, flags);
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

    return task->fds[fd].ops.write(task->fds[fd].ctx, buffer_kptr, len, flags);

}

int64_t syscall_ioctl(uint64_t fd, uint64_t ioctl, uint64_t args, uint64_t arg_count) {
    task_t* task = get_active_task();

    if (fd >= MAX_TASK_FDS) {
        return -1;
    }

    if ((!task->fds[fd].valid) ||
         task->fds[fd].ops.ioctl != NULL) {
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

        return task->fds[fd].ops.ioctl(task->fds[fd].ctx, ioctl, args_kptr, arg_count);
    } else {
        return task->fds[fd].ops.ioctl(task->fds[fd].ctx, ioctl, NULL, 0);
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

    return task->fds[fd].ops.close(task->fds[fd].ctx);
}
