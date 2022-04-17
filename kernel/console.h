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

#endif
