
#include <stdint.h>
#include <string.h>

#include "stdlib/bitutils.h"
#include "drivers/pl011_uart.h"
#include "kernel/assert.h"
#include "kernel/time.h"

#include "kernel/console.h"

fd_ops_t* s_driver_ops = NULL;
void* s_driver_ctx;

void console_putc(const char c) {
    //pl011_putc(VIRT_UART_VMEM, c);
    if (s_driver_ops != NULL) {
        s_driver_ops->write(s_driver_ctx, (const uint8_t*)&c, 1, 0);
    }
}

void console_endl(void) {
    console_putc('\n');
}

void console_write(const char* s) {
    //pl011_puts(VIRT_UART_VMEM, s);
    if (s_driver_ops != NULL) {
        int64_t len = strnlen(s, 4096);
        s_driver_ops->write(s_driver_ctx, (const uint8_t*)s, len, 0);
    }
}

void console_write_len(const char* s, uint64_t len) {
    if (s_driver_ops != NULL) {
        s_driver_ops->write(s_driver_ctx, (const uint8_t*)s, len, 0);
    }
    /*
    while (len > 0) {
        pl011_putc(VIRT_UART_VMEM, *s);
        s++;
        len--;
    }
    */
}

void console_flush(void) {
    // No-op
}

char console_getchar(void) {
    ASSERT(s_driver_ops != NULL);
    uint8_t buf;
    s_driver_ops->read(s_driver_ctx, &buf, 1, 0);
    return buf;
}

void console_add_driver(fd_ops_t* driver_ops, void* driver_ctx) {
    s_driver_ops = driver_ops;
    s_driver_ctx = driver_ctx;
}


static console_log_level_t s_max_level = LOG_DEBUG;

void console_set_log_level(console_log_level_t level) {
    if (level <= LOG_DEBUG) {
        s_max_level = level;
    }
}

void console_log(console_log_level_t level, const char* fmt, ...) {
    const char* log_strings[] = {
        "CRIT", "ERROR", "WARNING",
        "INFO", "DEBUG"
    };
    if (level <= s_max_level) {
        console_printf("[");
        uint64_t curr_time_us = time_get_bootns() / 1000;
        console_write_dec_fixed(curr_time_us, 6, 10);
        console_printf("] %s: ", log_strings[level]);
        va_list args;
        va_start(args, fmt);
        console_printf_helper(fmt, args);
        va_end(args);

        console_endl();
        console_flush();
    }
}