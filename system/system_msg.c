
#include <stdint.h>
#include <string.h>

#include "system/system_msg.h"
#include "system/system_lib.h"
#include "kernel/syscall.h"

static module_union_handlers_t s_handlers;
static module_class_t s_class;

void system_register_handler(module_union_handlers_t handlers, module_class_t class) {
    s_handlers = handlers;
    switch (class) {
        case MOD_CLASS_VFS:
        case MOD_CLASS_FS:
            s_class = class;
            break;
        default:
            s_class = MOD_CLASS_INVALID;
            break;
    }
}

int64_t system_get_dst(module_class_t class, char* subclass) {
    uint64_t x1, x2;
    x1 = 0;
    x2 = 0;
    if (strnlen(subclass, 16) <= 8) {
        strncpy((char*)&x1, subclass, 8);
        x2 = 0;
    } else {
        strncpy((char*)&x1, subclass, 8);
        strncpy((char*)&x2, &subclass[8], 8);
    }

    int64_t ret;
    SYSCALL_CALL_RET(SYSCALL_GETMOD, class, x1, x2, 0, ret)
    return ret;
}

int64_t system_send_msg(system_msg* msg) {
    uint64_t x0, x1, x2, x3;
    char* msg_8 = (char*)msg;
    memcpy(&x0, &msg_8[0], sizeof(uint64_t));
    memcpy(&x1, &msg_8[8], sizeof(uint64_t));
    memcpy(&x2, &msg_8[16], sizeof(uint64_t));
    memcpy(&x3, &msg_8[24], sizeof(uint64_t));

    int64_t ret;
    SYSCALL_CALL_RET(SYSCALL_SENDMSG, x0, x1, x2, x3, ret)
    return ret;
}

int64_t system_recv_msg_raw(system_msg* msg) {
    uint64_t msg_len = sizeof(system_msg);
    
    int64_t ret;
    SYSCALL_CALL_RET(SYSCALL_GETMSGS, (uintptr_t)msg, msg_len, 0, 0, ret)
    return ret;
}

void system_send_msg_vfs(system_msg* msg) {
    module_vfs_handlers_t h = s_handlers.vfs;
    switch (msg->port) {
        case MOD_VFS_INFO:
            h.info((system_msg_payload*)msg);
            break;
        case MOD_VFS_GETINFO:
            h.getinfo((system_msg_payload*)msg);
            break;
        default:
            break;
    }
}

void system_send_msg_fs(system_msg* msg) {
    module_fs_handlers_t h = s_handlers.fs;
    switch (msg->port) {
        case MOD_FS_INFO:
            h.info((system_msg_payload*)msg);
            break;
        case MOD_FS_GETINFO:
            h.getinfo((system_msg_payload*)msg);
            break;
        default:
            break;
    }
}

int64_t system_recv_msg(void) {
    system_msg msg;
    int64_t ret;

    ret = system_recv_msg_raw(&msg);
    if (ret < 0) {
        return ret;
    }

    switch (s_class) {
        case MOD_CLASS_VFS:
            system_send_msg_vfs(&msg);
            break;
        case MOD_CLASS_FS:
            system_send_msg_fs(&msg);
            break;
        default:
            break;
    }

    return 0;
}


