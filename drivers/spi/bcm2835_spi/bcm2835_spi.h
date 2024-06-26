
#pragma once

#define BCM2711_SPI_CS (0x00)
#define BCM2711_SPI_FIFO (0x04)
#define BCM2711_SPI_CLK (0x08)
#define BCM2711_SPI_DLEN (0x0c)
#define BCM2711_SPI_LTOH (0x10)
#define BCM2711_SPI_DC (0x14)

#define BCM2711_SPI_CS_LEN_LONG BIT(25)
#define BCM2711_SPI_CS_DMA_LEN BIT(24)
#define BCM2711_SPI_CS_CSPOL2 BIT(23)
#define BCM2711_SPI_CS_CSPOL1 BIT(22)
#define BCM2711_SPI_CS_CSPOL0 BIT(21)
#define BCM2711_SPI_CS_RXF BIT(20)
#define BCM2711_SPI_CS_RXR BIT(19)
#define BCM2711_SPI_CS_TXD BIT(18)
#define BCM2711_SPI_CS_RXD BIT(17)
#define BCM2711_SPI_CS_DONE BIT(16)
#define BCM2711_SPI_CS_LEN BIT(13)
#define BCM2711_SPI_CS_REN BIT(12)
#define BCM2711_SPI_CS_ADCS BIT(11)
#define BCM2711_SPI_CS_INTR BIT(10)
#define BCM2711_SPI_CS_INTD BIT(9)
#define BCM2711_SPI_CS_DMAEN BIT(8)
#define BCM2711_SPI_CS_TA BIT(7)
#define BCM2711_SPI_CS_CSPOL BIT(6)
#define BCM2711_SPI_CS_CLEAR_RX BIT(5)
#define BCM2711_SPI_CS_CLEAR_TX BIT(4)
#define BCM2711_SPI_CS_CPOL BIT(3)
#define BCM2711_SPI_CS_CPHA BIT(2)
#define BCM2711_SPI_CS_CS0 (0)
#define BCM2711_SPI_CS_CS1 BIT(1)
#define BCM2711_SPI_CS_CS2 BIT(2)
#define BCM2711_SPI_CS_CS(x) (x & 3)

#define BCM2711_SPI_DC_RPANIC_MASK (0xff << 24)
#define BCM2711_SPI_DC_RPANIC(x) ((x & 0xff) << 24)
#define BCM2711_SPI_DC_RDREQ_MASK (0xff << 16)
#define BCM2711_SPI_DC_RDREQ(x) ((x & 0xff) << 16)
#define BCM2711_SPI_DC_TPANIC_MASK (0xff << 8)
#define BCM2711_SPI_DC_TPANIC(x) ((x & 0xff) << 8)
#define BCM2711_SPI_DC_TDREQ_MASK (0xff)
#define BCM2711_SPI_DC_TDREQ(x) ((x & 0xff))