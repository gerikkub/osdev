
#ifndef __LIBVIRTIO__H__
#define __LIBVIRTIO__H__

#include <stdint.h>
#include <stdbool.h>

#include "kernel/lib/libpci.h"
#include "stdlib/malloc.h"

typedef struct __attribute__((__packed__)) {
    uint8_t cap;
    uint8_t next;
    uint8_t len;
    uint8_t type;
    uint8_t bar;
    uint8_t res0[3];
    uint32_t bar_offset;
    uint32_t bar_len;
} pci_virtio_capability_t;

typedef struct __attribute__((__packed__)) {
    pci_virtio_capability_t cap;
    uint32_t notify_off_multiplier;
} pci_virtio_notify_capability_t;

enum {
    VIRTIO_PCI_CAP_UNKNOWN = 0,
    VIRTIO_PCI_CAP_COMMON_CFG = 1,
    VIRTIO_PCI_CAP_NOTIFY_CFG = 2,
    VIRTIO_PCI_CAP_ISR_CFG = 3,
    VIRTIO_PCI_CAP_DEVICE_CFG = 4,
    VIRTIO_PCI_CAP_PCI_CFG = 5,
    VIRTIO_PCI_CAP_MAX = 6,
};

typedef struct __attribute__((__packed__)) {
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
    uint64_t queue_desc;
    uint64_t queue_driver;
    uint64_t queue_device;
} pci_virtio_common_cfg_t;

enum {
// Reserved Feature Bits
    VIRTIO_F_NOTIFY_ON_EMPTY = 24,
    VIRTIO_F_ANY_LAYOUT = 27,
    VIRTIO_F_RING_INDIRECT_DESC = 28,
    VIRTIO_F_RING_EVENT_IDX = 29,
    VIRTIO_F_VERSION_1 = 32,
    VIRTIO_F_ACCESS_PLATFORM = 33,
    VIRTIO_F_RING_PACKED = 34,
    VIRTIO_F_IN_ORDER = 35,
    VIRTIO_F_ORDER_PLATFORM = 36,
    VIRTIO_F_SR_IOV = 37,
    VIRTIO_F_NOTIFICATION_DATA = 38
};

enum {
    VIRTIO_DEVICE_ID_RESERVED = 0,
    VIRTIO_DEVICE_ID_NETWORK = 1,
    VIRTIO_DEVICE_ID_BLOCK = 2,
    VIRTIO_DEVICE_ID_CONSOLE = 3,
    VIRTIO_DEVICE_ID_ENTROPY = 4,
    VIRTIO_DEVICE_ID_MEM_BALLON = 5,
    VIRTIO_DEVICE_ID_IOMEMORY = 6,
    VIRTIO_DEVICE_ID_RPMSG = 7,
    // ...
    VIRTIO_DEVICE_ID_GPU = 16,
    VIRTIO_DEVICE_ID_TIMER = 17,
    VIRTIO_DEVICE_ID_INPUT = 18,
};

enum {
    VIRTIO_BLK_F_SIZE_MAX = 1,
    VIRTIO_BLK_F_SEG_MAX = 2,
    VIRTIO_BLK_F_GEOMETRY = 4,
    VIRTIO_BLK_F_RO = 5,
    VIRTIO_BLK_F_BLK_SIZE = 6,
    VIRTIO_BLK_F_FLUSH = 9,
    VIRTIO_BLK_F_TOPOLOGY = 10,
    VIRTIO_BLK_F_CONFIG_WCE = 11,
    VIRTIO_BLK_F_DISCARD = 13,
    VIRTIO_BLK_F_WRITE_ZEROES = 14
};

enum {
    VIRTIO_STATUS_ACKNOWLEGE = 1,
    VIRTIO_STATUS_DRIVER = 2,
    VIRTIO_STATUS_DRIVER_OK = 4,
    VIRTIO_STATUS_FEATURES_OK = 8,
    VIRTIO_STATUS_DEVICE_NEEDS_RESET = 64,
    VRITIO_STATUS_FAILED = 128
};


enum {
    VIRTQ_DESC_F_NEXT = 1,
    VIRTQ_DESC_F_WRITE = 2,
    VIRTQ_DESC_F_INDIRECT = 4
};

typedef struct __attribute__((__packed__)) {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} virtio_virtq_desc_t;

enum {
    VIRTQ_VAIL_F_NO_INTERRUPT = 1
};

typedef struct __attribute__((__packed__)) {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
} virtio_virtq_avail_t;

typedef struct __attribute__((__packed__)) {
    uint32_t id;
    uint32_t len;
} virtio_virtq_used_elem_t;

typedef struct __attribute__((__packed__)) {
    uint16_t flags;
    uint16_t idx;
    virtio_virtq_used_elem_t ring[];
} virtio_virtq_used_t;

typedef struct {
    uint16_t queue_size;
    uint16_t queue_num;
    uintptr_t desc_phy;
    virtio_virtq_desc_t* desc_ptr;
    uintptr_t avail_phy;
    virtio_virtq_avail_t* avail_ptr;
    uintptr_t used_phy;
    virtio_virtq_used_t* used_ptr;

    uint16_t queue_notify_off;
    uint16_t last_used_idx;

    void* buffer_pool;
    uintptr_t buffer_pool_phy;
    uint64_t buffer_pool_size;
    malloc_state_t buffer_malloc_state;

    int32_t intid;
} virtio_virtq_ctx_t;

typedef struct {
    void* ptr;
    uint64_t len;
} virtio_virtq_buffer_t;

pci_virtio_capability_t* virtio_get_capability(pci_device_ctx_t* device_ctx, uint64_t cap);

uint64_t virtio_get_features(pci_virtio_common_cfg_t* cfg);
void virtio_set_features(pci_virtio_common_cfg_t* cfg, uint64_t features);
uint8_t virtio_get_status(pci_virtio_common_cfg_t* cfg);
void virtio_set_status(pci_virtio_common_cfg_t* cfg, uint8_t status);
void virtio_alloc_queue(pci_virtio_common_cfg_t* cfg,
                        uint64_t queue_num, uint64_t queue_size,
                        uint64_t pool_size,
                        virtio_virtq_ctx_t* queue_out,
                        uint64_t msix_idx);

bool virtio_get_buffer(virtio_virtq_ctx_t* queue_ctx, uint64_t buffer_len, uintptr_t* buffer_out);
void virtio_return_buffer(virtio_virtq_ctx_t* queue_ctx, void* buffer_ptr);

bool virtio_virtq_send(virtio_virtq_ctx_t* queue_ctx,
                       virtio_virtq_buffer_t* write_buffers,
                       uint64_t num_write_buffers,
                       virtio_virtq_buffer_t* read_buffers,
                       uint64_t num_read_buffers);
                    
bool virtio_poll_virtq(virtio_virtq_ctx_t* queue_ctx, bool block);
bool virtio_poll_virtq_irq(virtio_virtq_ctx_t* queue_ctx);

void virtio_virtq_notify(pci_device_ctx_t* ctx, virtio_virtq_ctx_t* queue_ctx);

void print_pci_capability_qemu(pci_device_ctx_t* device_ctx, pci_vendor_capability_t* cap_ptr);

void print_qemu_capability_common(pci_device_ctx_t* device_ctx, pci_virtio_capability_t* cap_ptr);

void print_virtio_feature_bits(uint32_t feat_low, uint32_t feat_high, uint64_t device_id);

#endif