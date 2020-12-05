
#ifndef __PRINTF_H__
#define __PRINTF_H__

#include <stdarg.h>

void console_write_unum(uint64_t num);
void console_write_num(int64_t num);
void console_write_hex(uint64_t hex);
void console_write_hex_fixed(uint64_t hex, uint8_t digits);

/**
 * Usage:
 * %s - string
 * %d - int64_t
 * %u - uint64_t
 * %x - hex
 * %[0-9]+x - hex with fixed width
 * %% - %
 */

void console_printf(const char* fmt, ...);

typedef enum {
    LOG_EMERG = 0,
    LOG_ALERT = 1,
    LOG_CRIT = 2,
    LOG_ERROR = 3,
    LOG_WARNING = 4,
    LOG_NOTICE = 5,
    LOG_INFO = 6,
    LOG_DEBUG = 7
} console_log_level_t;

void console_set_log_level(console_log_level_t level);

void console_log(console_log_level_t level, const char* fmt, ...);

#endif
