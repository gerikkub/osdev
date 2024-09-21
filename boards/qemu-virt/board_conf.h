
#ifndef __BOARD_CONF_H__
#define __BOARD_CONF_H__

#include <stdint.h>
#define DTB_VMEM ((uintptr_t)0)

#define EARLY_CON_PHY_BASE 0x8000006000ULL
#define EARLY_CON_VIRT (0x8000006000ULL + 0xFFFF000000000000)
#define EARLY_PCI_PHY_BASE (0x4010000000 + 0x18000)
#define EARLY_PCI_VIRT (EARLY_PCI_PHY_BASE + 0xFFFF000000000000)


#endif
