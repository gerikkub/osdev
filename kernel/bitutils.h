
#ifndef __UTIL_H__
#define __UTIL_H__

#define NULL 0

#define BIT(x) (1UL << (x))

#define BITSIZE(x) BIT(x)

#define WRITE_SYS_REG(reg, x) \
    asm ("msr " # reg ", %[value]" : : [value] "r" (x))

#define READ_SYS_REG(reg, x) \
    asm ("mrs %[value], " # reg : [value] "=r" (x))

#endif

