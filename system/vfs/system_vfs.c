
#include <stdint.h>
#include <string.h>

#include "system/system_lib.h"
#include "system/system_msg.h"
#include "system/system_console.h"
#include "system/system_assert.h"

#include "kernel/syscall.h"
#include "kernel/messages.h"
#include "kernel/modules.h"

#include "stdlib/printf.h"

static uint64_t my_id;

void vfs_info(system_msg_payload* msg) {
    console_printf("Vfs Info: %c\n", msg->payload[0]);
    console_flush();

    int64_t ext2_dst = system_get_dst(MOD_CLASS_FS, "ext2");
    SYS_ASSERT(ext2_dst >= 0);

    system_msg_payload info = {
        .type = MSG_TYPE_PAYLOAD,
        .flags = 0,
        .dst = ext2_dst,
        .src = my_id,
        .port = MOD_FS_GETINFO,
        .payload = {0}
    }; 

    int64_t ret = system_send_msg((system_msg*)&info);
    SYS_ASSERT(ret >= 0);
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
    SYS_ASSERT(me_src >= 0);
    my_id = me_src;

    module_union_handlers_t u_handlers;
    u_handlers.vfs = handlers;
    system_register_handler(u_handlers, MOD_CLASS_VFS);

    console_printf("VFS\n");
    console_flush();

    int64_t ext2_dst = system_get_dst(MOD_CLASS_FS, "ext2");
    SYS_ASSERT(ext2_dst >= 0);

    system_msg_payload info = {
        .type = MSG_TYPE_PAYLOAD,
        .flags = 0,
        .dst = ext2_dst,
        .src = my_id,
        .port = MOD_FS_GETINFO,
        .payload = {0}
    }; 

    int64_t ret = system_send_msg((system_msg*)&info);
    SYS_ASSERT(ret >= 0);

    while(1) {
        system_recv_msg();
    }
}