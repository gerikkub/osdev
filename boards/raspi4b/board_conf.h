
#ifndef __BOARD_CONF_H__
#define __BOARD_CONF_H__

#include <stdint.h>

#include "drivers/pl011_uart.h"

#define DTB_VMEM ((uintptr_t)0)

#define PL011_VMEM ((PL011_Struct*) 0xFFFF00003F201000)
#define PL011_PHY ((PL011_Struct*) 0x3F201000)

#define PERIPHERAL_BASE_PHY (0xFE000000)
#define PERIPHERAL_BASE ((void*)0xFFFF0000FE000000)

#define AUX_BASE_PHY (PERIPHERAL_BASE_PHY + 0x215000)
#define AUX_BASE (PERIPHERAL_BASE + 0x215000)


#endif