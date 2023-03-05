
#include <stdint.h>

#include "pl011_uart.h"
#include "kernel/drivers.h"
#include "kernel/console.h"

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

int64_t pl011_ops_write(void* ctx, const uint8_t* buffer, const int64_t size, const uint64_t flags) {
    for (int64_t idx = 0; idx < size; idx++) {
        pl011_putc(ctx, buffer[idx]);
    }

    return size;
}

int64_t pl011_ops_read(void* ctx, uint8_t* buffer, const int64_t size, const uint64_t flags) {
    char c = pl011_getc(ctx);

    if (size > 0) {
        *buffer = c;
        return 1;
    } else {
        return 0;
    }
}


fd_ops_t pl011_ops = {
    .write = pl011_ops_write,
    .read = pl011_ops_read
};

void pl011_discover(void* ctx) {
    //console_add_driver(&pl011_ops, VIRT_UART);
}

void pl011_register(void) {
    discovery_register_t reg = {
        .type = DRIVER_DISCOVERY_DTB,
        .dtb = {
            .compat = "arm,pl011\0arm,primecell"
        },
        .ctxfunc = pl011_discover
    };
    register_driver(&reg);
}

REGISTER_DRIVER(pl011)