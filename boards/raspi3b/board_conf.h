
#ifndef __BOARD_CONF_H__
#define __BOARD_CONF_H__

#include <stdint.h>

#include "drivers/pl011_uart.h"

#define DTB_VMEM ((uintptr_t)0)

#define PL011_VMEM ((PL011_Struct*) 0xFFFF00003F201000)
#define PL011_PHY ((PL011_Struct*) 0x3F201000)


#endif