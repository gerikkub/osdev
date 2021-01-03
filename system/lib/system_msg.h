
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
    msg_payload_fun ioctl;
    msg_memory_fun ctx;
} module_generic_handlers_t;

typedef struct {
    msg_payload_fun dummy;
} module_vfs_handlers_t;

typedef struct {
    msg_memory_fun printstr;
} module_fs_handlers_t;

typedef struct {
    module_generic_handlers_t generic;
    union {
        module_vfs_handlers_t vfs;
        module_fs_handlers_t fs;
    } class;
} module_handlers_t;

void system_register_handler(module_handlers_t handlers, module_class_t class);
int64_t system_send_msg(system_msg_t* msg);
int64_t system_recv_msg(void);

int64_t system_startmod_class(module_class_t class, char* subclass);
int64_t system_startmod_compat(char* compat);
int64_t system_startmod_pci(uint16_t vendor, uint16_t device);


#endif