
#include <stdint.h>

#include "kernel/fd.h"
#include "kernel/console.h"

#include "stdlib/bitutils.h"

int64_t virtio_pci_early_console_write(void* ctx, const uint8_t* buffer, const int64_t size, const uint64_t flags) {
    uint32_t* emerg_wr_addr = ctx;
    int64_t size_copy = size;

    while (size_copy > 0) {
        *emerg_wr_addr = (uint32_t)(*buffer);
        buffer++;
        size_copy--;
    }

    return size;
}

int64_t virtio_pci_early_console_read(void* ctx, uint8_t* buffer, const int64_t size, const uint64_t flags) {
    return 0;
}

static fd_ops_t s_early_ops = {
    .read = virtio_pci_early_console_read,
    .write = virtio_pci_early_console_write,
    .ioctl = NULL,
    .close = NULL
};

void virtio_pci_early_console_init(uint32_t* emerg_wr_addr) {
    console_add_driver(&s_early_ops, emerg_wr_addr);
}
