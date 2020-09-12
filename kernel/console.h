#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include <stdint.h>

void console_putc(char c);
void console_endl(void);
void console_write(char* s);
void console_write_unum(uint64_t num);
void console_write_num(int64_t num);
void console_write_hex(uint64_t hex);
void console_write_hex_fixed(uint64_t hex, uint8_t digits);

#endif