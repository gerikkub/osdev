
#ifndef __SYSTEM_CONSOLE_H__
#define __SYSTEM_CONSOLE_H__

#define CONSOLE SYSTEM

#include <stdint.h>
#include <stdbool.h>

#include "stdlib/printf.h"

void console_open(void);
void console_putc(const char c);
void console_endl(void);
void console_write(const char* s);
void console_write_len(const char* s, uint64_t len);
void console_flush(void);

int64_t console_read(char* buffer, uint64_t len);


#endif
