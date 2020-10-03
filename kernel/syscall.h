
#ifndef __SYSCALL_H__
#define __SYSCALL_H__

void syscall_init(void);

#define MAX_SYSCALL_NUM 64

#define SYSCALL_YIELD 0
#define SYSCALL_PRINT 1

#endif