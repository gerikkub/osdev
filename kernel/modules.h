
#ifndef __MODULES_H__
#define __MODULES_H__

#include "include/k_modules.h"

void modules_init_list(void);

void modules_start(void);

int64_t syscall_startmod(uint64_t startmod_struct,
                         uint64_t x1,
                         uint64_t x2,
                         uint64_t x3);

#endif