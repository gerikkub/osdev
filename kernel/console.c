
#include <stdint.h>

#include "drivers/pl011_uart.h"

void console_putc(char c) {
    pl011_putc(VIRT_UART_VMEM, c);
}

void console_endl(void) {
    console_putc('\n');
}

void console_write(char* s) {
    pl011_puts(VIRT_UART_VMEM, s);
}

void console_write_unum(uint64_t num) {

    const char* num_lookup = "012345679";
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

    uint8_t idx = 15;
    while ((hex >> (idx*4) & 0xF) == 0 && idx > 0) {
        idx--;
    }

    if (idx == 0) {
        console_putc('0');
    } else {
        do {
            console_putc(hex_lookup[(hex >> (idx*4)) & 0xF]);
            idx--;
        } while (idx > 0);
        console_putc(hex_lookup[hex & 0xF]);
    }
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
