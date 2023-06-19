
#ifndef __UTIL_H__
#define __UTIL_H__

#include <stdint.h>
#include <stdbool.h>

#ifndef NULL
#define NULL 0
#endif

#define BIT(x) (1UL << (x))
#define BITSIZE(x) BIT(x)

#define ARR_ELEMENTS(arr) (sizeof(arr)/sizeof(arr[0]))

#define ALIGN_DOWN(x, align) (((x) + align - 1) % align)

#define WRITE_SYS_REG(reg, x) WRITE_SYS_REG_HELPER(reg, x)
#define READ_SYS_REG(reg, x) READ_SYS_REG_HELPER(reg, x)

#define WRITE_SYS_REG_HELPER(reg, x) \
    do { \
    uint64_t __val = x; \
    asm ("msr " # reg ", %[value]" : : [value] "r" (__val)); \
    } while(0)

#define READ_SYS_REG_HELPER(reg, x) \
    asm ("mrs %[value], " # reg : [value] "=r" (x))


#define GET_ABS_SYM(x, sym) \
    asm ("movz %[out], #:abs_g3:" #sym  "\n \
          movk %[out], #:abs_g2_nc:" #sym  "\n \
          movk %[out], #:abs_g1_nc:" #sym  "\n \
          movk %[out], #:abs_g0_nc:" #sym \
          : [out] "=r" (x))

#define MEM_ISB() asm volatile ("isb SY")
#define MEM_DMB() asm volatile ("dmb SY")
#define MEM_DSB() asm volatile ("dsb SY")

uint16_t en_swap_16(uint16_t a);
uint32_t en_swap_32(uint32_t a);
uint64_t en_swap_64(uint64_t a);

/**
 * hextoi64
 * Transfrom a hex string to a 64-bit integer
 * 
 * s1: Character string (0-9a-fA-F)
 * n: Max number of characters to read
 * valid: Option boolean pointer to signal an invalid conversion
 * 
 * return: integer result of conversion
 */
uint64_t hextou64(const char* s1, uint64_t n, bool* valid);

int64_t strtoi64(const char* str, bool* valid);

#endif

