
#ifndef __EXEC_H__
#define __EXEC_H__

#include <stdint.h>

uint64_t exec_user_task(const char* device, const char* path, const char* name, char** argv);

#endif
