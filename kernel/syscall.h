
#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include "include/k_syscall.h"

void syscall_init(void);

extern const char* syscall_print_table[MAX_SYSCALL_NUM];


#endif