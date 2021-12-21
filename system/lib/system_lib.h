#ifndef __SYSTEM_LIB_H__
#define __SYSTEM_LIB_H__

#include <stdint.h>
#include <stdbool.h>

#define SYSCALL_CALL(NUM, x0, x1, x2, x3) \
{ \
   uint64_t dummy; \
   SYSCALL_CALL_HELPER(NUM, x0, x1, x2, x3, dummy) \
   (void)dummy; \
}

#define SYSCALL_CALL_RET(NUM, x0, x1, x2, x3, ret) SYSCALL_CALL_HELPER(NUM, x0, x1, x2, x3, ret)

#define SYSCALL_CALL_HELPER(NUM, x0, x1, x2, x3, ret_arg) \
    { \
    uint64_t _x0, _x1, _x2, _x3; \
    uint64_t _ret; \
    _x0 = x0; \
    _x1 = x1; \
    _x2 = x2; \
    _x3 = x3; \
 \
    asm volatile ("mov x0, %[arg1]\n \
                   mov x1, %[arg2]\n \
                   mov x2, %[arg3]\n \
                   mov x3, %[arg4]\n \
                   svc " # NUM  "\n \
                   mov %[ret], x0" \
                   : [ret] "=r" (_ret) : \
                   [arg1] "r" (_x0), [arg2] "r" (_x1), \
                   [arg3] "r" (_x2), [arg4] "r" (_x3)); \
 \
    ret_arg = _ret; \
    }

void system_init(void);

void* system_map_device(uintptr_t device, uint64_t len);
bool system_map_anyphy(uintptr_t len, uintptr_t* phy_out, uintptr_t* virt_out);

void system_yield(void);

int64_t system_exec(const char* device, const char* path, const char* name, char** const argv);

#endif