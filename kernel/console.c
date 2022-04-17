
#include <stdint.h>
#include <string.h>

#include "stdlib/bitutils.h"
#include "drivers/pl011_uart.h"
#include "kernel/assert.h"

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