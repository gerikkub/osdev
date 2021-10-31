
#ifndef __SYSTEM_FILE_H__
#define __SYSTEM_FILE_H__

#include <stdint.h>

int64_t system_open(const char* device, const char* path, const uint64_t flags);
int64_t system_read(const int64_t fd, const void* buffer, const uint64_t len, const uint64_t flags);
int64_t system_write(const int64_t fd, void* buffer, const uint64_t len, const uint64_t flags);
int64_t system_ioctl(const int64_t fd, const uint64_t ioctl, const uint64_t* args, const uint64_t arg_count);
int64_t system_close(const int64_t fd);

#endif
