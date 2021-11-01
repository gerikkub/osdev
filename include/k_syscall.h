
#ifndef __K_SYSCALL_H__
#define __K_SYSCALL_H__

#define MAX_SYSCALL_NUM 64

#define SYSCALL_YIELD 0
#define SYSCALL_SENDMSG 1
#define SYSCALL_GETMSGS 2
#define SYSCALL_STARTMOD 3
#define SYSCALL_MAPDEV 4
#define SYSCALL_SBRK 5
#define SYSCALL_OPEN 6
#define SYSCALL_READ 7
#define SYSCALL_WRITE 8
#define SYSCALL_IOCTL 9
#define SYSCALL_CLOSE 10



enum {
    SYSCALL_ERROR_OK = 0,
    SYSCALL_ERROR_BADARG = -1,
    SYSCALL_ERROR_NORESOURCE = -2,
    SYSCALL_ERROR_NOSPACE = -3,
};

enum {
    SYSCALL_MAPDEV_ANYPHY = 1
};

typedef struct {
    uintptr_t virt_addr;
    uintptr_t phy_addr;
} syscall_mapdev_ctx_t;


#endif