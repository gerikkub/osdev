
#ifndef __K_SPI_H__
#define __K_SPI_H__

#include <stdint.h>

typedef struct {
    int64_t clk_hz;
    uint64_t flags;
    #define SPI_DEVICE_FLAG_CS_MASK (0xFF)
    #define SPI_DEVICE_FLAG_CS(x) (x & 0xFF)
    #define SPI_DEVICE_FLAG_CS_AH BIT(8)
    #define SPI_DEVICE_FLAG_CPOL_LOW (0)
    #define SPI_DEVICE_FLAG_CPOL_HIGH BIT(9)
    #define SPI_DEVICE_FLAG_CPHA_MID (0)
    #define SPI_DEVICE_FLAG_CPHA_BEGIN BIT(10)
    #define SPI_DEVICE_FLAG_EV_DONE BIT(11)
} k_spi_device_t;

#endif
