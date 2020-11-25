
#ifndef __SYSTEM_MSG_H__
#define __SYSTEM_MSG_H__

#include <stdint.h>
#include "kernel/modules.h"

typedef struct {
    uint8_t type;
    uint8_t flags;
    uint16_t dst;
    uint16_t src;
    uint16_t port;
    uint8_t msg[24];
} system_msg;

typedef struct {
    uint8_t type;
    uint8_t flags;
    uint16_t dst;
    uint16_t src;
    uint16_t port;
    uint8_t payload[24];
} system_msg_payload;

typedef struct {
    uint8_t type;
    uint8_t flags;
    uint16_t dst;
    uint16_t src;
    uint16_t port;
    uint64_t ptr[2];
    uint8_t payload[8];
} system_msg_memory;

typedef void (*msg_payload_fun)(system_msg_payload*);

typedef struct {
    msg_payload_fun info;
    msg_payload_fun getinfo;
} module_vfs_handlers_t;

typedef struct {
    msg_payload_fun info;
    msg_payload_fun getinfo;
} module_fs_handlers_t;

typedef union {
    module_vfs_handlers_t vfs;
    module_fs_handlers_t fs;
} module_union_handlers_t;

void system_register_handler(module_union_handlers_t handlers, module_class_t class);
int64_t system_get_dst(module_class_t class, char* subclass);
int64_t system_send_msg(system_msg* msg);
int64_t system_recv_msg(void);



#endif