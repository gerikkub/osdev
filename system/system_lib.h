#ifndef __SYSTEM_LIB_H__
#define __SYSTEM_LIB_H__

#include <stdint.h>

//#define SYSCALL_CALL(NUM, x0, x1, x2, x3)

#define SYSCALL_CALL(NUM, x0, x1, x2, x3) SYSCALL_CALL_HELPER(NUM, x0, x1, x2, x3)

#define SYSCALL_CALL_HELPER(NUM, x0, x1, x2, x3) \
    { \
    uint64_t _x0, _x1, _x2, _x3; \
    uint64_t _ret; \
    _x0 = x0; \
    _x1 = x1; \
    _x2 = x2; \
    _x3 = x3; \
 \
    asm("mov x0, %[arg1]\n \
         mov x1, %[arg2]\n \
         mov x2, %[arg3]\n \
         mov x3, %[arg4]\n \
         svc " # NUM  "\n \
         mov %[ret], x0" \
         : [ret] "=r" (_ret) : \
         [arg1] "r" (_x0), [arg2] "r" (_x1), \
         [arg3] "r" (_x2), [arg4] "r" (_x3)); \
 \
    }


#endif