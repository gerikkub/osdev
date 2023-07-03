
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "kernel/syscall.h"
#include "kernel/task.h"
#include "kernel/assert.h"
#include "kernel/console.h"
#include "kernel/vmem.h"
#include "kernel/kernelspace.h"
#include "kernel/messages.h"
#include "kernel/modules.h"
#include "kernel/kmalloc.h"
#include "kernel/exec.h"
#include "kernel/lib/vmalloc.h"
#include "kernel/net/net_api.h"
#include "kernel/net/udp_socket.h"
#include "kernel/net/tcp_socket.h"
#include "kernel/net/tcp_bind.h"

#include "include/k_net_api.h"

int64_t syscall_socket(uint64_t socket_struct_ptr,
                       uint64_t x1,
                       uint64_t x2,
                       uint64_t x3) {

    task_t* task = get_active_task();

    uint64_t socket_struct_phy;
    bool walk_ok;
    walk_ok = vmem_walk_table(task->low_vm_table, socket_struct_ptr, &socket_struct_phy);
    if (!walk_ok) {
        return SYSCALL_ERROR_BADARG;
    }

    k_create_socket_t* create_socket_ctx = (k_create_socket_t*)(PHY_TO_KSPACE(socket_struct_phy));

    int64_t fd_num = find_open_fd(task);
    if (fd_num < 0) {
        return -1;
    }

    int64_t ret;

    switch (create_socket_ctx->socket_type) {
        case SYSCALL_SOCKET_UDP4:
            ret = net_udp_create_socket(create_socket_ctx, &task->fds[fd_num].ops, &task->fds[fd_num].ctx, &task->fds[fd_num]);
            break;
        case SYSCALL_SOCKET_TCP4:
            ret = net_tcp_create_socket(create_socket_ctx, &task->fds[fd_num].ops, &task->fds[fd_num].ctx, &task->fds[fd_num]);
            break;
        default:
            ret = -1;
            break;
    }

    if (ret >= 0) {
        task->fds[fd_num].valid = true;
        return fd_num;
    } else {
        return -1;
    }
}

int64_t syscall_bind(uint64_t bind_struct_ptr,
                     uint64_t x1,
                     uint64_t x2,
                     uint64_t x3) {

    task_t* task = get_active_task();

    uint64_t bind_struct_phy;
    bool walk_ok;
    walk_ok = vmem_walk_table(task->low_vm_table, bind_struct_ptr, &bind_struct_phy);
    if (!walk_ok) {
        return SYSCALL_ERROR_BADARG;
    }

    k_bind_port_t* bind_port_ctx = (k_bind_port_t*)(PHY_TO_KSPACE(bind_struct_phy));

    int64_t fd_num = find_open_fd(task);
    if (fd_num < 0) {
        return -1;
    }

    int64_t ret;

    switch (bind_port_ctx->bind_type) {
        case SYSCALL_BIND_TCP4:
            ret = net_tcp_bind_port(bind_port_ctx, &task->fds[fd_num].ops, &task->fds[fd_num].ctx, &task->fds[fd_num]);
            break;
        default:
            ret = -1;
            break;
    }

    if (ret >= 0) {
        task->fds[fd_num].valid = true;
        return fd_num;
    } else {
        return -1;
    }
}