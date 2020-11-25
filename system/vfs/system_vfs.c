
#include <stdint.h>
#include <string.h>

#include "system/system_lib.h"
#include "system/system_msg.h"
#include "kernel/syscall.h"
#include "kernel/messages.h"
#include "kernel/modules.h"

static uint64_t my_id;

void vfs_info(system_msg_payload* msg) {
    char my_str[12];
    char* my_str_ro = "  VFS_INFO\n";
    strncpy(my_str, my_str_ro, 12);
    my_str[0] = msg->payload[0];
    SYSCALL_CALL(SYSCALL_PRINT, (uintptr_t)my_str, 0, 0, 0);

    int64_t ext2_dst = system_get_dst(MOD_CLASS_FS, "ext2");
    if (ext2_dst < 0) {
        char* my_str2 = "VFS ERROR\n";
        SYSCALL_CALL(SYSCALL_PRINT, (uintptr_t)my_str2, 0, 0, 0);
        while (1) {}
    }

    system_msg_payload info = {
        .type = MSG_TYPE_PAYLOAD,
        .flags = 0,
        .dst = ext2_dst,
        .src = my_id,
        .port = MOD_FS_GETINFO,
        .payload = {0}
    }; 

    int64_t ret = system_send_msg((system_msg*)&info);
    if (ret < 0) {
        char* my_str3 = "VFS SEND FAILED\n";
        SYSCALL_CALL(SYSCALL_PRINT, (uintptr_t)my_str3, 0, 0, 0);
        while (1) {}
    }
}

void vfs_getinfo(system_msg_payload* msg) {

    system_msg_payload info = {
        .type = MSG_TYPE_PAYLOAD,
        .flags = 0,
        .dst = msg->src,
        .src = my_id,
        .port = MOD_FS_INFO,
        .payload = {'V', 0}
    };

    system_send_msg((system_msg*)&info);
}

static module_vfs_handlers_t handlers = {
    .info = vfs_info,
    .getinfo = vfs_getinfo
};

void main(void* parameters) {

    int64_t me_src;
    me_src = system_get_dst(MOD_CLASS_VFS, "");
    if (me_src < 0) {
        char* err_str = "VFS ERR\n";
        SYSCALL_CALL(SYSCALL_PRINT, (uintptr_t)err_str, 0, 0, 0); 
        while (1) {}
    }
    my_id = me_src;

    module_union_handlers_t u_handlers;
    u_handlers.vfs = handlers;
    system_register_handler(u_handlers, MOD_CLASS_VFS);

    char* my_str = "VFS\n";
    SYSCALL_CALL(SYSCALL_PRINT, (uintptr_t)my_str, 0, 0, 0);

    int64_t ext2_dst = system_get_dst(MOD_CLASS_FS, "ext2");
    if (ext2_dst < 0) {
        char* my_str2 = "VFS ERROR\n";
        SYSCALL_CALL(SYSCALL_PRINT, (uintptr_t)my_str2, 0, 0, 0);
        while (1) {}
    }

    system_msg_payload info = {
        .type = MSG_TYPE_PAYLOAD,
        .flags = 0,
        .dst = ext2_dst,
        .src = my_id,
        .port = MOD_FS_GETINFO,
        .payload = {0}
    }; 

    int64_t ret = system_send_msg((system_msg*)&info);
    if (ret < 0) {
        char* my_str3 = "VFS SEND FAILED\n";
        SYSCALL_CALL(SYSCALL_PRINT, (uintptr_t)my_str3, 0, 0, 0);
        while (1) {}
    }

    while(1) {
        system_recv_msg();
    }
}
