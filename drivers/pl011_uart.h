#ifndef __PL001_UART_H__
#define __PL001_UART_H__

#include <stdint.h>

typedef struct __attribute__((__packed__)) {
    volatile uint32_t dr;
    volatile uint32_t sr_cr;
    volatile uint32_t dummy_11;
    volatile uint32_t dummy_12;
    volatile uint32_t dummy_13;
    volatile uint32_t dummy_14;
    volatile uint32_t fr;
    volatile uint32_t dummy_3;
    volatile uint32_t ilpr;
    volatile uint32_t ibrd;
    volatile uint32_t fbrd;
    volatile uint32_t tlcr_h;
    volatile uint32_t tcr;
    volatile uint32_t tifls;
    volatile uint32_t timsc;
    volatile uint32_t tris;
    volatile uint32_t tmis;
    volatile uint32_t ticr;
    volatile uint32_t dmacr;
} PL011_Struct ;

#define VIRT_UART ((PL011_Struct*) 0x09000000)
#define VIRT_UART_VMEM ((PL011_Struct*) 0xFFFF000009000000)
// #define VIRT_UART ((PL011_Struct*) 0x40000100)

#define PL011_FR_RI (1 << 8)
#define PL011_FR_TXFE (1 << 7)
#define PL011_FR_RXFF (1 << 6)
#define PL011_FR_TXFF (1 << 5)
#define PL011_FR_RXFE (1 << 4)
#define PL011_FR_BUSY (1 << 3)
#define PL011_FR_DCD (1 << 2)
#define PL011_FR_DSR (1 << 1)
#define PL011_FR_CTS (1 << 0)

#define PL011_TLCR_H_SPS (1 << 7)
#define PL011_TLCR_H_WLEN_1 (1 << 6)
#define PL011_TLCR_H_WLEN_0 (1 << 5)
#define PL011_TLCR_H_FEN (1 << 4)
#define PL011_TLCR_H_STP2 (1 << 3)
#define PL011_TLCR_H_EPS (1 << 2)
#define PL011_TLCR_H_PEN (1 << 1)
#define PL011_TLCR_H_BRK (1 << 0)

#define PL011_CR_CTSEN (1 << 15)
#define PL011_CR_RTSEN (1 << 14)
#define PL011_CR_OUT2 (1 << 13)
#define PL011_CR_OUT1 (1 << 12)
#define PL011_CR_RTS (1 << 11)
#define PL011_CR_DTR (1 << 10)
#define PL011_CR_RXE (1 << 9)
#define PL011_CR_TXE (1 << 8)
#define PL011_CR_LBE (1 << 7)
#define PL011_CR_SIRLP (1 << 2)
#define PL011_CR_SIREN (1 << 1)
#define PL011_CR_UARTEN (1 << 0)

void pl011_init(PL011_Struct* dev);

void pl011_putc(PL011_Struct* dev, char c);

void pl011_puts(PL011_Struct* dev, char* str);

#endif
