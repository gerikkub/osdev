
#ifndef __LIBVIRTIO__H__
#define __LIBVIRTIO__H__

#include <stdint.h>
#include <stdbool.h>

#include "kernel/lib/libpci.h"
#include "stdlib/malloc.h"

#define VIRTIO_PCI_CAP_TYPE (3)
#define VIRTIO_PCI_CAP_BAR (4)
#define VIRTIO_PCI_CAP_BAR_OFF (8)
#define VIRTIO_PCI_CAP_BAR_LEN (12)
#define VIRTIO_PCI_CAP_NOTIFY_OFF_MUL (16)
/*
typedef struct __attribute__((__packed__)) {
    pci_virtio_capability_t cap;
    uint32_t notify_off_multiplier;
} pci_virtio_notify_capability_t;
*/

enum {
    VIRTIO_PCI_CAP_UNKNOWN = 0,
    VIRTIO_PCI_CAP_COMMON_CFG = 1,
    VIRTIO_PCI_CAP_NOTIFY_CFG = 2,
    VIRTIO_PCI_CAP_ISR_CFG = 3,
    VIRTIO_PCI_CAP_DEVICE_CFG = 4,
    VIRTIO_PCI_CAP_PCI_CFG = 5,
    VIRTIO_PCI_CAP_MAX = 6,
};

#define VIRTIO_MSI_NO_VECTOR (0xFFFF)

/*
#define VIRTIO_PCI_COMMON_CFG_DEV_FEAT_SEL (0)
#define VIRTIO_PCI_COMMON_CFG_DEV_FEAT (4)
#define VIRTIO_PCI_COMMON_CFG_DRI_FEAT_SEL (8)
#define VIRTIO_PCI_COMMON_CFG_DRI_FEAT (12)
#define VIRTIO_PCI_COMMON_CFG_MSIX_CONF (16)
#define VIRTIO_PCI_COMMON_CFG_NUM_Q (18)
#define VIRTIO_PCI_COMMON_CFG_DEV_STAT (20)
#define VIRTIO_PCI_COMMON_CFG_CFG_GEN (21)
#define VIRTIO_PCI_COMMON_CFG_Q_SEL (22)
#define VIRTIO_PCI_COMMON_CFG_Q_SIZE (24)
#define VIRTIO_PCI_COMMON_CFG_Q_MSIX_VEC (26)
#define VIRTIO_PCI_COMMON_CFG_Q_EN (28)
#define VIRTIO_PCI_COMMON_CFG_Q_NOTIFY_OFF (30)
#define VIRTIO_PCI_COMMON_CFG_Q_DESC (32)
#define VIRTIO_PCI_COMMON_CFG_Q_DRI (40)
#define VIRTIO_PCI_COMMON_CFG_Q_DEV (48)
*/

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
    VIRTIO_CONSOLE_F_SIZE = 0,
    VIRTIO_CONSOLE_F_MULTIPORT = 1,
    VIRTIO_CONSOLE_F_EMERG_WRITE = 2
};

enum {
    VIRTIO_NET_F_CSUM = 0,
    VIRTIO_NET_F_GUEST_CSUM = 1,
    VIRTIO_NET_F_CTRL_GUEST_OFFLOADS = 2,
    VIRTIO_NET_F_MTU = 3,
    VIRTIO_NET_F_MAC = 5,
    VIRTIO_NET_F_GUEST_TSO4 = 7,
    VIRTIO_NET_F_GUEST_TSO6 = 8,
    VIRTIO_NET_F_GUEST_ECN = 9,
    VIRTIO_NET_F_GUEST_UFO = 10,
    VIRTIO_NET_F_HOST_TSO4 = 11,
    VIRTIO_NET_F_HOST_TSO6 = 12,
    VIRTIO_NET_F_HOST_ECN = 13,
    VIRTIO_NET_F_HOST_UFO = 14,
    VIRTIO_NET_F_MRG_RXBUF = 15,
    VIRTIO_NET_F_STATUS = 16,
    VIRTIO_NET_F_CTRL_VQ = 17,
    VIRTIO_NET_F_CTRL_RX = 18,
    VIRTIO_NET_F_CTRL_VLAN = 19,
    VIRTIO_NET_F_GUEST_ANNOUNCE = 21,
    VIRTIO_NET_F_MQ = 22,
    VIRTIO_NET_F_CTRL_MAC_ADDRR = 23,
    VIRTIO_NET_F_RSC_EXT = 61,
    VIRTIO_NET_F_STANDBY = 62
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
    VIRTIO_QUEUE_NET_RECEIVEQ1 = 0,
    VIRTIO_QUEUE_NET_TRANSMITQ1 = 1
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
    llist_head_t wait_queue;
    int32_t intid;
} virtio_virtq_shared_irq_ctx_t;

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

    bool should_wakeup;
} virtio_virtq_ctx_t;

typedef struct {
    void* ptr;
    uint64_t len;
} virtio_virtq_buffer_t;

typedef struct {
    uint8_t mac[6];
    uint16_t status;
    uint16_t max_virtqueue_pairs;
    uint16_t mtu;
} virtio_net_config_t;

enum {
    VIRTIO_NET_HDR_F_NEEDS_CSUM = 1,
    VIRTIO_NET_HDR_F_DATA_VALID = 2,
    VIRTIO_NET_HDR_F_RSC_INFO = 3
};

typedef struct __attribute__((__packed__)) {
    uint8_t flags;
    uint8_t gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
    uint16_t num_buffers;
} virtio_net_hdr_t;

pci_cap_t* virtio_get_capability(pci_device_ctx_t* device_ctx, uint8_t cap_type);

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
uint64_t virtio_poll_virtq_irq(virtio_virtq_ctx_t* queue_ctx, virtio_virtq_shared_irq_ctx_t* irq_ctx);
void virtio_handle_irq(virtio_virtq_shared_irq_ctx_t* irq_ctx);
int64_t virtio_get_used_elem(virtio_virtq_ctx_t* queue_ctx, int64_t desc_idx);
int64_t virtio_get_last_used_elem(virtio_virtq_ctx_t* queue_ctx);

void virtio_virtq_notify(pci_device_ctx_t* ctx, virtio_virtq_ctx_t* queue_ctx);

void print_pci_capability_virtio(pci_device_ctx_t* device_ctx, pci_cap_t* cap);

void print_virtio_capability_common(pci_device_ctx_t* device_ctx, pci_cap_t* cap);

void print_virtio_feature_bits(uint32_t feat_low, uint32_t feat_high, uint64_t device_id);

void virtio_init_pci_caps(pci_device_ctx_t* pci_ctx);
bool virtio_init_with_features(pci_device_ctx_t* pci_ctx, uint64_t features);

uint64_t virtio_poll_virtq_block(virtio_virtq_ctx_t* queue_ctx);

#define VIRTIO_HAS_FEATURE(features, feat) (features & (1 << feat))

#endif
