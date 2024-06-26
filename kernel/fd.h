
#ifndef __FD_H__
#define __FD_H__

#include <stdint.h>
#include <stdbool.h>

typedef int64_t (*fd_read_op)(void* ctx, uint8_t* buffer, const int64_t size, const uint64_t flags);
typedef int64_t (*fd_write_op)(void* ctx, const uint8_t* buffer, const int64_t size, const uint64_t flags);
typedef int64_t (*fd_ioctl_op)(void* ctx, const uint64_t ioctl, const uint64_t* args, const uint64_t arg_count);
typedef int64_t (*fd_close_op)(void* ctx);

typedef struct {
    fd_read_op read;
    fd_write_op write;
    fd_ioctl_op ioctl;
    fd_close_op close;
} fd_ops_t;

struct task_t_;

typedef struct {
    bool valid;
    void* ctx;
    fd_ops_t ops;

    uint64_t ready;
    struct task_t_* task;
} fd_ctx_t;

int64_t syscall_open(uint64_t device, uint64_t path, uint64_t flags, uint64_t dummy);
int64_t syscall_read(uint64_t fd, uint64_t buffer, uint64_t len, uint64_t flags);
int64_t syscall_write(uint64_t fd, uint64_t buffer, uint64_t len, uint64_t flags);
int64_t syscall_ioctl(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3);
int64_t syscall_close(uint64_t fd, uint64_t arg1, uint64_t arg2, uint64_t arg3);

int64_t fd_call_read(fd_ctx_t* fd_ctx, uint8_t* buffer, const int64_t size, const uint64_t flags);
int64_t fd_call_write(fd_ctx_t* fd_ctx, const uint8_t* buffer, const int64_t size, const uint64_t flags);
int64_t fd_call_ioctl(fd_ctx_t* fd_ctx, const uint64_t ioctl, const uint64_t *args, const uint64_t arg_count);
int64_t fd_call_ioctl_ptr(fd_ctx_t* fd_ctx, const uint64_t ioctl, void* arg);
int64_t fd_call_close(fd_ctx_t* fd_ctx);

#endif