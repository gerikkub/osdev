
#ifndef __VIRTIO_PCI_EARLY_CONSOLE_H__
#define __VIRTIO_PCI_EARLY_CONSOLE_H__

#include <stdint.h>

#define EARLY_CON_BASE_OFFSET 8

void virtio_pci_early_console_init(uint32_t* emerg_wr_addr);

#endif
