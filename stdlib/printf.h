
#ifndef __PRINTF_H__
#define __PRINTF_H__

#include <stdarg.h>

void console_write_unum(uint64_t num);
void console_write_num(int64_t num);
void console_write_hex(uint64_t hex);
void console_write_hex_fixed(uint64_t hex, uint8_t digits);
void console_write_dec_fixed(uint64_t num, uint64_t point_log10, uint64_t digits);

/**
 * Usage:
 * %s - string
 * %d - int64_t
 * %u - uint64_t
 * %x - hex
 * %[0-9]+x - hex with fixed width
 * %% - %
 */

void console_printf_helper(const char* fmt, va_list args);
void console_printf(const char* fmt, ...);

int snprintf(char* str, uint64_t size, const char* format, ...);

#endif
