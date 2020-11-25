
#include <stdint.h>
#include <string.h>

#include "system/system_lib.h"
#include "system/system_msg.h"
#include "kernel/syscall.h"
#include "kernel/messages.h"
#include "kernel/modules.h"

static uint64_t my_id;

void ext2_info(system_msg_payload* msg) {
    char my_str[13];
    char* my_str_ro = "  EXT2_INFO\n";
    strncpy(my_str, my_str_ro, 13);
    my_str[0] = msg->payload[0];
    SYSCALL_CALL(SYSCALL_PRINT, (uintptr_t)my_str, 0, 0, 0);

    int64_t vfs_dst = system_get_dst(MOD_CLASS_VFS, "");
    if (vfs_dst < 0) {
        char* my_str2 = "EXT2 ERROR\n";
        SYSCALL_CALL(SYSCALL_PRINT, (uintptr_t)my_str2, 0, 0, 0);
        while (1) {}
    }

    system_msg_payload info = {
        .type = MSG_TYPE_PAYLOAD,
        .flags = 0,
        .dst = vfs_dst,
        .src = my_id,
        .port = MOD_FS_GETINFO,
        .payload = {0}
    }; 

    int64_t ret = system_send_msg((system_msg*)&info);
    if (ret < 0) {
        char* my_str3 = "EXT2 SEND FAILED\n";
        SYSCALL_CALL(SYSCALL_PRINT, (uintptr_t)my_str3, 0, 0, 0);
        while (1) {}
    }
}

void ext2_getinfo(system_msg_payload* msg) {

    system_msg_payload info = {
        .type = MSG_TYPE_PAYLOAD,
        .flags = 0,
        .dst = msg->src,
        .src = my_id,
        .port = MOD_FS_INFO,
        .payload = {'E', 0}
    };

    system_send_msg((system_msg*)&info);
}

static module_fs_handlers_t handlers = {
    .info = ext2_info,
    .getinfo = ext2_getinfo
};


void main(void* parameters) {

    int64_t me_src;
    me_src = system_get_dst(MOD_CLASS_FS, "ext2");
    if (me_src < 0) {
        char* err_str = "EXT2 ERR\n";
        SYSCALL_CALL(SYSCALL_PRINT, (uintptr_t)err_str, 0, 0, 0); 
        while (1) {}
    }
    my_id = me_src;

    module_union_handlers_t u_handlers;
    u_handlers.fs = handlers;
    system_register_handler(u_handlers, MOD_CLASS_FS);

    char* my_str = "EXT2\n";
    SYSCALL_CALL(SYSCALL_PRINT, (uintptr_t)my_str, 0, 0, 0);

    int64_t vfs_dst = system_get_dst(MOD_CLASS_VFS, "");
    if (vfs_dst < 0) {
        char* my_str2 = "EXT2 ERROR\n";
        SYSCALL_CALL(SYSCALL_PRINT, (uintptr_t)my_str2, 0, 0, 0);
        while (1) {}
    }

    system_msg_payload info = {
        .type = MSG_TYPE_PAYLOAD,
        .flags = 0,
        .dst = vfs_dst,
        .src = my_id,
        .port = MOD_FS_GETINFO,
        .payload = {0}
    }; 

    int64_t ret = system_send_msg((system_msg*)&info);
    if (ret < 0) {
        char* my_str3 = "EXT2 SEND FAILED\n";
        SYSCALL_CALL(SYSCALL_PRINT, (uintptr_t)my_str3, 0, 0, 0);
        while (1) {}
    }

    while(1) {
        system_recv_msg();
    }
}
