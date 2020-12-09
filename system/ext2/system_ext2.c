
#include <stdint.h>
#include <string.h>

#include "system/system_lib.h"
#include "system/system_msg.h"
#include "system/system_console.h"
#include "system/system_assert.h"

#include "include/k_syscall.h"
#include "include/k_messages.h"
#include "include/k_modules.h"

#include "stdlib/printf.h"

static uint64_t my_id;

void ext2_info(system_msg_payload_t* msg) {
    console_printf("Ext2 Info: %c\n", msg->payload[0]);
    console_flush();

}

void ext2_getinfo(system_msg_payload_t* msg) {

    system_msg_payload_t info = {
        .type = MSG_TYPE_PAYLOAD,
        .flags = 0,
        .dst = msg->src,
        .src = my_id,
        .port = MOD_GENERIC_INFO,
        .payload = {'E', 0}
    };

    system_send_msg((system_msg_t*)&info);
}

void ext2_printstr(system_msg_memory_t* msg) {

    console_printf("Msg Recv'd: %s\n", msg->ptr);
    console_flush();
}

void ext2_dtb(system_msg_memory_t* msg) {
    console_printf("Fake Ext2 node name: %s\n", msg->ptr);
    console_flush();
}

static module_handlers_t s_handlers = {
    { // generic
        .info = ext2_info,
        .getinfo = ext2_getinfo,
        .ioctl = NULL,
        .dtb = ext2_dtb
    },
    { // class
        .fs = {
            .printstr = ext2_printstr
        }
    }
};

void main(uint64_t my_tid, module_ctx_t* ctx) {

    console_write("Ext2 Hello\n");
    console_flush();

    my_id = my_tid;

    system_register_handler(s_handlers, MOD_CLASS_FS);

    console_printf("EXT2\n");
    console_flush();

    int64_t vfs_dst = system_startmod_class(MOD_CLASS_VFS, "");
    SYS_ASSERT(vfs_dst >= 0);

    /*
    system_msg_payload_t info = {
        .type = MSG_TYPE_PAYLOAD,
        .flags = 0,
        .dst = vfs_dst,
        .src = my_id,
        .port = MOD_FS_GETINFO,
        .payload = {0}
    }; 

    int64_t ret = system_send_msg((system_msg_t*)&info);
    SYS_ASSERT(ret >= 0);
    */

    while(1) {
        system_recv_msg();
    }
}
