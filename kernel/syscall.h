
#ifndef __SYSCALL_H__
#define __SYSCALL_H__

void syscall_init(void);

#define MAX_SYSCALL_NUM 64

#define SYSCALL_YIELD 0
#define SYSCALL_PRINT 1
#define SYSCALL_SENDMSG 2
#define SYSCALL_GETMSGS 3
#define SYSCALL_GETMOD 4

enum {
    SYSCALL_ERROR_OK = 0,
    SYSCALL_ERROR_BADARG = -1,
    SYSCALL_ERROR_NORESOURCE = -2,
    SYSCALL_ERROR_NOSPACE = -3,
};


#endif