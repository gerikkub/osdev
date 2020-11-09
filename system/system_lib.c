
#include <stdint.h>

/*
uint64_t syscall_call(uint64_t num, uint64_t x0, uint64_t x1, uint64_t x2, uint64_t x3) {
    uint64_t ret;

    asm("mov x0, %[arg1]\n \
         mov x1, %[arg2]\n \
         mov x2, %[arg3]\n \
         mov x3, %[arg4]\n \
         svc %[num]\n \
         mov %[ret], x0"
         : [ret] "=r" (ret) :
         [arg1] "r" (x0), [arg2] "r" (x1),
         [arg3] "r" (x2), [arg4] "r" (x3),
         [num] "r" (num));

    return ret;
}
*/