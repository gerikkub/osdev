
#ifndef __SYSFS_H__
#define __SYSFS_H__

#include <stdint.h>

#include <kernel/fd.h>
#include <kernel/fs/file.h>

typedef struct {
    fd_ops_t ops;
    void* ctx;
} sysfs_file_fd_ctx_t;

typedef void* (*sysfs_open_op)(void);

void sysfs_create_file(char* name, sysfs_open_op open_op, fd_ops_t* ops);
void sysfs_ro_file_helper(void* data_str, uint64_t data_str_len, file_ctx_t* file_ctx_out);

void sysfs_task_init(void);
void sysfs_time_init(void);
void sysfs_vmalloc_init(void);
void sysfs_profile_init(void);

void sysfs_register(void);


#endif