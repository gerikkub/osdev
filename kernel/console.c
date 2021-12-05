
#include <stdint.h>

#include "drivers/pl011_uart.h"

void console_putc(const char c) {
    pl011_putc(VIRT_UART_VMEM, c);
}

void console_endl(void) {
    console_putc('\n');
}

void console_write(const char* s) {
    pl011_puts(VIRT_UART_VMEM, s);
}

void console_write_len(const char* s, uint64_t len) {
    while (len > 0) {
        pl011_putc(VIRT_UART_VMEM, *s);
        s++;
        len--;
    }
}

void console_flush(void) {
    // No-op
}

char console_getchar(void) {
    return pl011_getc(VIRT_UART_VMEM);
}