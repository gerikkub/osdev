
#include <stdint.h>

#include "pl011_uart.h"

void pl011_init(PL011_Struct* dev) {
    dev->tlcr_h = PL011_TLCR_H_FEN |
                  PL011_TLCR_H_WLEN_0 |
                  PL011_TLCR_H_WLEN_1;

    dev->tcr = PL011_CR_TXE |
               PL011_CR_RXE |
               PL011_CR_UARTEN;
}


void pl011_putc(PL011_Struct* dev, const char c) {

    while (dev->fr & PL011_FR_TXFF);

    dev->dr = (uint32_t)c;
}

void pl011_puts(PL011_Struct* dev, const char* str) {

    while(*str) {
        pl011_putc(dev, *str);
        str++;
    }
}

char pl011_getc(PL011_Struct* dev) {

    while ((dev->fr & PL011_FR_RXFE) != 0) {}

    return (char)(dev->dr & 0xFF);
}

void _putchar(char character) {
    pl011_putc(VIRT_UART, character);
}

char _getchar(void) {
    return pl011_getc(VIRT_UART);
}