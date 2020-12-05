
#ifndef __MODULES_H__
#define __MODULES_H__

#include "include/k_modules.h"

void modules_init_list(void);

void modules_start(void);

int64_t syscall_getmod(uint64_t class,
                       uint64_t subclass0,
                       uint64_t subclass1,
                       uint64_t x3);

#endif