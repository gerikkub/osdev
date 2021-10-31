
#include <stdint.h>

#include "system/lib/system_lib.h"
#include "include/k_syscall.h"


int64_t system_open(const char* device, const char* path, const uint64_t flags) {
    int64_t ret;
    SYSCALL_CALL_RET(SYSCALL_OPEN, (uintptr_t)device, (uintptr_t)path, flags, 0, ret);
    return ret;
}

int64_t system_read(const int64_t fd, const void* buffer, const uint64_t len, const uint64_t flags) {
    int64_t ret;
    SYSCALL_CALL_RET(SYSCALL_READ, fd, (uintptr_t)buffer, len, flags, ret);
    return ret;
}

int64_t system_write(const int64_t fd, void* buffer, const uint64_t len, const uint64_t flags) {
    int64_t ret;
    SYSCALL_CALL_RET(SYSCALL_WRITE, fd, (uintptr_t)buffer, len, flags, ret);
    return ret;
}

int64_t system_ioctl(const int64_t fd, const uint64_t ioctl, const uint64_t* args, const uint64_t arg_count) {
    int64_t ret;
    SYSCALL_CALL_RET(SYSCALL_IOCTL, fd, ioctl, (uintptr_t)args, arg_count, ret);
    return ret;
}

int64_t system_close(const uint64_t fd) {
    int64_t ret;
    SYSCALL_CALL_RET(SYSCALL_CLOSE, fd, 0, 0, 0, ret);
    return ret;
}