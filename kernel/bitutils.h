
#ifndef __UTIL_H__
#define __UTIL_H__

#ifndef NULL
#define NULL 0
#endif

#define BIT(x) (1UL << (x))

#define BITSIZE(x) BIT(x)

#define WRITE_SYS_REG(reg, x) \
    do { \
    uint64_t __val = x; \
    asm ("msr " # reg ", %[value]" : : [value] "r" (__val)); \
    } while(0)

#define READ_SYS_REG(reg, x) \
    asm ("mrs %[value], " # reg : [value] "=r" (x))

#endif

