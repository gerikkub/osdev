
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>

#include "stdlib/printf.h"

void console_putc(const char c);
void console_endl(void);
void console_write(const char* s);
void console_write_len(const char* s, uint64_t len);
void console_flush(void);

void console_write_unum(uint64_t num) {

    const char* num_lookup = "0123456789";
    uint64_t comp = 10000000000000000000ULL;

    while (num < comp && comp > 1) {
        comp /= 10;
    }

    while (comp > 0) {
        uint8_t dividend = num / comp;
        console_putc(num_lookup[dividend]);
        num = num - (dividend * comp);
        comp /= 10;
    }
}

void console_write_num(int64_t num) {
    if (num & (1ULL << 63)) {
        console_putc('-');
        console_write_unum((~num) + 1);
    } else {
        console_write_unum(num);
    }
}

void console_write_hex(uint64_t hex) {
    const char* hex_lookup = "0123456789abcdef";

    int8_t idx = 15;
    while ((hex >> (idx*4) & 0xF) == 0 && idx > 0) {
        idx--;
    }

    do {
        console_putc(hex_lookup[(hex >> (idx*4)) & 0xF]);
        idx--;
    } while (idx >= 0);
}

void console_write_hex_fixed(uint64_t hex, uint8_t digits) {
    const char* hex_lookup = "0123456789abcdef";
    uint8_t idx = digits - 1;

    do {
        console_putc(hex_lookup[(hex >> (idx*4)) & 0xF]);
        idx--;
    } while (idx > 0);
    console_putc(hex_lookup[hex & 0xF]);
}

void console_printf_helper(const char* fmt, va_list args) {

    while (*fmt != '\0') {
        if (*fmt == '%') {
            // Special format print
            uint64_t digits = 0;
            char* s;
            int64_t i;
            uint64_t u;
            fmt++;
            do {
            switch(*fmt) {
                case '\0':
                    break;
                case 's':
                    s = va_arg(args, char*);
                    console_write(s);
                    fmt++;
                    break;
                case 'c':
                    u = va_arg(args, uint64_t);
                    console_putc((char)u);
                    fmt++;
                    break;
                case 'd':
                    i = va_arg(args, int64_t);
                    console_write_num(i);
                    fmt++;
                    break;
                case 'u':
                    u = va_arg(args, uint64_t);
                    console_write_unum(u);
                    fmt++;
                    break;
                case 'x':
                    u = va_arg(args, uint64_t);
                    if (digits != 0) {
                        console_write_hex_fixed(u, digits);
                    } else {
                        console_write_hex(u);
                    }
                    fmt++;
                    break;
                case '%':
                    console_putc('%');
                    fmt++;
                    break;
                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                    digits = 0;
                    while (*fmt >= '0' &&
                           *fmt <= '9') {
                        digits *= 10;
                        uint64_t digit = *fmt - '0';
                        digits += digit;
                        fmt++;
                    }
                    // Repeat the switch statement, but this time with
                    // a valid digit count
                    continue;
                default:
                    // Anything else, just ignore the leading % sign
                    break;
            }
            break;
            } while (1);
        } else {
            // Print the next substring
            const char* start_ptr;

            start_ptr = fmt;

            while (*fmt != '\0' &&
                   *fmt != '%') {
                fmt++;
            }
            ptrdiff_t len = fmt - start_ptr;
            console_write_len(start_ptr, len);
        }
    }
}

void console_printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    console_printf_helper(fmt, args);
    va_end(args);
}

// static console_log_level_t s_max_level = LOG_DEBUG;
static console_log_level_t s_max_level = LOG_INFO;

void console_set_log_level(console_log_level_t level) {
    if (level <= LOG_DEBUG) {
        s_max_level = level;
    }
}

void console_log(console_log_level_t level, const char* fmt, ...) {
    const char* log_strings[] = {
        "EMERG", "ALERT", "CRIT",
        "ERROR", "WARNING", "NOTICE",
        "INFO", "DEBUG"
    };
    if (level <= s_max_level) {
        console_printf("[%s] ", log_strings[level]);
        va_list args;
        va_start(args, fmt);
        console_printf_helper(fmt, args);
        va_end(args);

        console_flush();
    }
}