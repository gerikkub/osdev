#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#define CONSOLE KERNEL

#include <stdint.h>
#include <stdbool.h>

#include "stdlib/printf.h"

#include "kernel/fd.h"

void console_putc(const char c);
void console_endl(void);
void console_write(const char* s);
void console_write_len(const char* s, uint64_t len);
void console_flush(void);

char console_getchar(void);

void console_add_driver(fd_ops_t* driver, void* ctx);

typedef enum {
    LOG_CRIT = 0,
    LOG_ERROR = 1,
    LOG_WARN = 2,
    LOG_INFO = 3,
    LOG_DEBUG = 4
} console_log_level_t;

void console_set_log_level(console_log_level_t level);

void console_log(console_log_level_t level, const char* fmt, ...);

#endif
