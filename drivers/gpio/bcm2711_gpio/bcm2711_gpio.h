
#pragma once


#define BCM2711_GPIO_FSET(x) ((x)*4)
#define BCM2711_GPIO_SET(x) (0x1c + (x)*4)
#define BCM2711_GPIO_CLR(x) (0x28 + (x)*4)
#define BCM2711_GPIO_LEV(x) (0x34 + (x)*4)
#define BCM2711_GPIO_EDS(x) (0x40 + (x)*4)
#define BCM2711_GPIO_REN(x) (0x4c + (x)*4)
#define BCM2711_GPIO_FEN(x) (0x58 + (x)*4)
#define BCM2711_GPIO_HEN(x) (0x64 + (x)*4)
#define BCM2711_GPIO_LEN(x) (0x70 + (x)*4)
#define BCM2711_GPIO_AREN(x) (0x7c + (x)*4)
#define BCM2711_GPIO_AFEN(x) (0x88 + (x)*4)
#define BCM2711_GPIO_PCTRL(x) (0xe4 + (x)*4)