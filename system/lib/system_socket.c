
#include <stdint.h>

#include "system/lib/system_lib.h"
#include "include/k_syscall.h"
#include "include/k_net_api.h"

int64_t system_socket(k_create_socket_t* socket_ptr) {
    int64_t ret;
    SYSCALL_CALL_RET(SYSCALL_SOCKET, (uintptr_t)socket_ptr, 0, 0, 0, ret);
    return ret;
}

int64_t system_bind(k_bind_port_t* bind_ptr) {
    int64_t ret;
    SYSCALL_CALL_RET(SYSCALL_BIND, (uintptr_t)bind_ptr, 0, 0, 0, ret);
    return ret;
}