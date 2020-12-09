
#include <stdint.h>
#include <string.h>

#include "system/system_msg.h"
#include "system/system_lib.h"
#include "system/system_assert.h"
#include "system/system_dtb.h"

#include "include/k_syscall.h"

static module_handlers_t s_handlers;
static module_class_t s_class;

void system_register_handler(module_handlers_t handlers, module_class_t class) {
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

int64_t system_startmod_class(module_class_t class, char* subclass) {
    module_startmod_t startmod;

    startmod.startsel = MOD_STARTSEL_CLASS;
    startmod.data.class_subclass.class = class;
    strncpy(startmod.data.class_subclass.subclass_name, subclass, MOD_SUBCLASS_STRLEN);

    int64_t ret;
    SYSCALL_CALL_RET(SYSCALL_STARTMOD, (uintptr_t)&startmod, 0, 0, 0, ret)
    return ret;
}

int64_t system_startmod_compat(char* compat) {
    SYS_ASSERT(compat != NULL);

    module_startmod_t startmod;

    startmod.startsel = MOD_STARTSEL_COMPAT;
    strncpy(startmod.data.compat.compat_name, compat, MAX_DTB_NODE_NAME);

    int64_t ret;
    SYSCALL_CALL_RET(SYSCALL_STARTMOD, (uintptr_t)&startmod, 0, 0, 0, ret)
    return ret;
}

int64_t system_send_msg(system_msg_t* msg) {
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

int64_t system_recv_msg_raw(system_msg_t* msg) {
    uint64_t msg_len = sizeof(system_msg_t);
    
    int64_t ret;
    SYSCALL_CALL_RET(SYSCALL_GETMSGS, (uintptr_t)msg, msg_len, 0, 0, ret)
    return ret;
}

void system_send_msg_generic(system_msg_t* msg) {
    module_generic_handlers_t* h = &s_handlers.generic;
    switch (msg->port) {
        case MOD_GENERIC_INFO:
            h->info((system_msg_payload_t*)msg);
            break;
        case MOD_GENERIC_GETINFO:
            h->getinfo((system_msg_payload_t*)msg);
            break;
        case MOD_GENERIC_DTB:
            h->dtb((system_msg_memory_t*)msg);
            break;
        default:
            break;
    }
}

void system_send_msg_vfs(system_msg_t* msg) {
    module_vfs_handlers_t h = s_handlers.class.vfs;
    switch (msg->port) {
        case MOD_VFS_DUMMY:
            h.dummy((system_msg_payload_t*)msg);
            break;
        default:
            break;
    }
}

void system_send_msg_fs(system_msg_t* msg) {
    module_fs_handlers_t h = s_handlers.class.fs;
    switch (msg->port) {
        case MOD_FS_STR:
            h.printstr((system_msg_memory_t*)msg);
            break;
        default:
            break;
    }
}

int64_t system_recv_msg(void) {
    system_msg_t msg;
    int64_t ret;

    ret = system_recv_msg_raw(&msg);
    if (ret < 0) {
        return ret;
    }

    if (msg.port < MAX_MOD_GENERIC_PORT) {
        system_send_msg_generic(&msg);
    } else {

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
    }

    return 0;
}


