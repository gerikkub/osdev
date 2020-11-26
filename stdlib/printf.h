
#ifndef __PRINTF_H__
#define __PRINTF_H__

#include <stdarg.h>

#ifndef CONSOLE
#error "No console defined. Console must be included before printf"
#endif

void console_write_unum(uint64_t num);
void console_write_num(int64_t num);
void console_write_hex(uint64_t hex);
void console_write_hex_fixed(uint64_t hex, uint8_t digits);

/**
 * Usage:
 * %s - string
 * %d - int64_t
 * %u - uint64_t
 * %h - hex
 * %[0-9]+h - hex with fixed width
 * %% - %
 */
void console_printf(const char* fmt, ...);

#endif