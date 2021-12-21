
#include <stdint.h>

#include "pl011_uart.h"

#include "kernel/interrupt/interrupt.h"

static void pl011_rx_handler(uint32_t intid, void* ctx) {
    PL011_Struct* dev = ctx;

    dev->ticr = 0xFF;
}

void pl011_init(PL011_Struct* dev) {
    dev->tlcr_h = PL011_TLCR_H_FEN |
                  PL011_TLCR_H_WLEN_0 |
                  PL011_TLCR_H_WLEN_1;

    dev->tcr = PL011_CR_TXE |
               PL011_CR_RXE |
               PL011_CR_UARTEN;

}

void pl011_init_rx(PL011_Struct* dev) {
    dev->timsc = PL011_MIS_RX;

    interrupt_register_irq_handler(PL011_IRQn, pl011_rx_handler, dev);
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

    // while ((dev->fr & PL011_FR_RXFE) != 0) {}

    // return (char)(dev->dr & 0xFF);
    uint32_t flags;
    char ret;
    uint64_t crit;
    BEGIN_CRITICAL(crit);
    flags = dev->fr;
    if (!(flags & PL011_FR_RXFE)) {
        ret = (char)(dev->dr & 0xFF);
    } else {
        interrupt_enable_irq(PL011_IRQn);
        interrupt_await_reset(PL011_IRQn);
    }
    END_CRITICAL(crit);

    if (!(flags & PL011_FR_RXFE)) {
        return ret;
    } else {
        interrupt_await(PL011_IRQn);
        ret = (char)(dev->dr & 0xFF);
        return ret;
    }
}

void _putchar(char character) {
    pl011_putc(VIRT_UART, character);
}

char _getchar(void) {
    return pl011_getc(VIRT_UART);
}