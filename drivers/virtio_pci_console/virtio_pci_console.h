
#ifndef __VIRTIO_PCI_CONSOLE__H__
#define __VIRTIO_PCI_CONSOLE__H__

#include <stdint.h>

#include "kernel/lib/libvirtio.h"
#include "kernel/lib/libpci.h"


typedef struct __attribute__((__packed__)) {
    uint16_t cols;
    uint16_t rows;
    uint32_t max_nr_ports;
    uint32_t emerg_wr;
} virtio_console_cfg_t;

typedef struct __attribute__((__packed__)) {
    uint32_t port; // "id" in the spec
    uint16_t event;
    uint16_t value;
} virtio_console_ctrl_msg_t;

enum {
    VIRTIO_QUEUE_CONSOLE_RECEIVEQ_0 = 0,
    VIRTIO_QUEUE_CONSOLE_TRANSMITQ_0 = 1,
    VIRTIO_QUEUE_CONSOLE_CTRL_RECEIVEQ = 2,
    VIRTIO_QUEUE_CONSOLE_CTRL_TRANSMITQ = 3,
};
#define VIRTIO_QUEUE_CONSOLE_RECEIVEQ_N(x) (4 + (x-1)*2)
#define VIRTIO_QUEUE_CONSOLE_TRANSMITQ_N(x) (5 + (x-1)*2)

enum {
    VIRTIO_CONSOLE_DEVICE_READY = 0,
    VIRTIO_CONSOLE_DEVICE_ADD = 1,
    VIRTIO_CONSOLE_DEVICE_REMOVE = 2,
    VIRTIO_CONSOLE_PORT_READY = 3,
    VIRTIO_CONSOLE_CONSOLE_PORT = 4,
    VIRTIO_CONSOLE_RESIZE = 5,
    VIRTIO_CONSOLE_PORT_OPEN = 6,
    VIRTIO_CONSOLE_PORT_NAME = 7,
    VIRTIO_CONSOLE_Max
};


#endif
