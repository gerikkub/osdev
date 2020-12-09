
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

void vfs_info(system_msg_payload_t* msg) {
    console_printf("Vfs Info: %c\n", msg->payload[0]);
    console_flush();
}

void vfs_getinfo(system_msg_payload_t* msg) {

    system_msg_payload_t info = {
        .type = MSG_TYPE_PAYLOAD,
        .flags = 0,
        .dst = msg->src,
        .src = my_id,
        .port = MOD_GENERIC_INFO,
        .payload = {'V', 0}
    };

    system_send_msg((system_msg_t*)&info);
}

static module_handlers_t s_handlers = {
    { // generic
        .info = vfs_info,
        .getinfo = vfs_getinfo,
        .ioctl = NULL,
        .dtb = NULL
    },
    { // class
        .vfs = {
            .dummy = NULL
        }
    }
};

void main(uint64_t my_tid, module_ctx_t* ctx) {

    my_id = my_tid;

    system_register_handler(s_handlers, MOD_CLASS_VFS);

    console_printf("VFS\n");
    console_flush();

    int64_t ext2_dst = system_startmod_class(MOD_CLASS_FS, "ext2");
    SYS_ASSERT(ext2_dst >= 0);

    const char* const str = "Hey Ext2!";
    uint64_t str_len = strlen(str) + 1;

    system_msg_memory_t str_msg = {
        .type = MSG_TYPE_MEMORY,
        .flags = 0,
        .dst = ext2_dst,
        .src = my_id,
        .port = MOD_FS_STR,
        .ptr = (uintptr_t)str,
        .len = str_len,
        .payload = {0}
    };
    int64_t ret;
    ret = system_send_msg((system_msg_t *)&str_msg);
    SYS_ASSERT(ret >= 0);

    /*
    system_msg_payload_t info = {
        .type = MSG_TYPE_PAYLOAD,
        .flags = 0,
        .dst = ext2_dst,
        .src = my_id,
        .port = MOD_FS_GETINFO,
        .payload = {0}
    }; 

    int64_t ret = system_send_msg((system_msg*)&info);
    SYS_ASSERT(ret >= 0);
    */

    while(1) {
        system_recv_msg();
    }
}
