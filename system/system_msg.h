
#ifndef __SYSTEM_MSG_H__
#define __SYSTEM_MSG_H__

#include <stdint.h>

#include "include/k_modules.h"
#include "include/k_messages.h"

typedef void (*msg_payload_fun)(system_msg_payload_t*);
typedef void (*msg_memory_fun)(system_msg_memory_t*);

typedef struct {
    msg_payload_fun info;
    msg_payload_fun getinfo;
} module_vfs_handlers_t;

typedef struct {
    msg_payload_fun info;
    msg_payload_fun getinfo;
    msg_memory_fun printstr;
} module_fs_handlers_t;

typedef union {
    module_vfs_handlers_t vfs;
    module_fs_handlers_t fs;
} module_union_handlers_t;

void system_register_handler(module_union_handlers_t handlers, module_class_t class);
int64_t system_get_dst(module_class_t class, char* subclass);
int64_t system_send_msg(system_msg_t* msg);
int64_t system_recv_msg(void);



#endif