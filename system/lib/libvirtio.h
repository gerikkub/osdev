
#ifndef __LIBVIRTIO__H__
#define __LIBVIRTIO__H__

#include <stdlib.h>
#include <stdbool.h>

#include "system/lib/libpci.h"

typedef struct __attribute__((__packed__)) {
    uint8_t cap;
    uint8_t next;
    uint8_t len;
    uint8_t type;
    uint8_t bar;
    uint8_t res0[3];
    uint32_t bar_offset;
    uint32_t bar_len;
} pci_qemu_capability_t;

typedef enum {
    VIRTIO_PCI_CAP_UNKNOWN = 0,
    VIRTIO_PCI_CAP_COMMON_CFG = 1,
    VIRTIO_PCI_CAP_NOTIFY_CFG = 2,
    VIRTIO_PCI_CAP_ISR_CFG = 3,
    VIRTIO_PCI_CAP_DEVICE_CFG = 4,
    VIRTIO_PCI_CAP_PCI_CFG = 5,
    VIRTIO_PCI_CAP_MAX = 6,
} pci_qemu_virtio_capability_t;

typedef struct {
    uint32_t device_feature_sel;
    uint32_t device_feature;
    uint32_t driver_feature_sel;
    uint32_t driver_feature;
    uint16_t msix_config;
    uint16_t num_queues;
    uint8_t device_status;
    uint8_t config_generation;

    uint16_t queue_select;
    uint16_t queue_size;
    uint16_t queue_msix_vector;
    uint16_t queue_enable;
    uint16_t queue_notify_off;
    uint16_t queue_desc;
    uint16_t queue_driver;
    uint16_t queue_device;
} pci_qemu_virtio_common_cfg_t;

void print_pci_capability_qemu(pci_device_ctx_t* device_ctx, pci_vendor_capability_t* cap_ptr);

void print_qemu_capability_common(pci_device_ctx_t* device_ctx, pci_qemu_capability_t* cap_ptr);

#endif