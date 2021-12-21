
#include <stdint.h>

#include "kernel/assert.h"
#include "kernel/console.h"
#include "kernel/lib/libpci.h"
#include "kernel/lib/libvirtio.h"
#include "kernel/lib/llist.h"
#include "kernel/lib/vmalloc.h"
#include "kernel/drivers.h"
#include "kernel/fd.h"
#include "kernel/sys_device.h"

#include "include/k_ioctl_common.h"

#include "stdlib/bitutils.h"
#include "stdlib/string.h"

typedef struct __attribute__((__packed__)) {
    uint64_t capacity;
    uint32_t size_max;
    uint32_t seg_max;
    struct virtio_blk_geometry {
        uint32_t cylinders;
        uint8_t heads;
        uint8_t sectors;
    } geometry;
    uint32_t blk_size;
    struct virtio_blk_topology {
        uint8_t physical_block_exp;
        uint8_t alignment_offset;
        uint16_t min_io_size;
        uint32_t opt_io_size;
    } topology;
    uint8_t writeback;
    uint8_t res0[3];
    uint32_t max_discard_sectors;
    uint32_t max_discard_seg;
    uint32_t discard_sector_alignment;
    uint32_t max_write_zeroes_sectors;
    uint32_t max_write_zeroes_seg;
    uint8_t write_zeroes_may_unmap;
    uint8_t res1[3];
} virtio_blk_config_t;

typedef struct {
    pci_device_ctx_t pci_device;
    virtio_virtq_ctx_t virtio_requestq;
    uint64_t device_pos;
    virtio_blk_config_t device_config;
    char name[MAX_SYS_DEVICE_NAME_LEN];
} blk_disk_ctx_t;

static llist_head_t s_blk_disks;
static uint64_t s_disk_counter;

enum {
    VIRTIO_QUEUE_BLOCK_REQUESTQ = 0
};

typedef struct __attribute__((__packed__)) {
    uint32_t type;
    uint32_t res;
    uint64_t sector;
} virtio_blk_req_header_t;

typedef struct __attribute__((__packed__)) {
    uint8_t status;
} virtio_blk_req_status_t;

enum {
    VIRTIO_BLK_T_IN = 0,
    VIRTIO_BLK_T_OUT = 1,
    VIRTIO_BLK_T_FLUSH = 4,
    VIRTIO_BLK_T_DISCARD = 11,
    VIRTIO_BLK_T_WRITE_ZEROES = 13
};

static void print_blk_config(pci_device_ctx_t* pci_ctx) {

    pci_virtio_capability_t* blk_cfg_cap;

    blk_cfg_cap = virtio_get_capability(pci_ctx, VIRTIO_PCI_CAP_DEVICE_CFG);
    ASSERT(blk_cfg_cap != NULL);

    virtio_blk_config_t* blk_config;

    blk_config = pci_ctx->bar[blk_cfg_cap->bar].vmem + blk_cfg_cap->bar_offset;

    console_printf("Disk Capacity: %u KBs\n", blk_config->capacity * 512 / 1024);
    console_flush();
}

static void init_blk_device(blk_disk_ctx_t* disk_ctx) {

    pci_device_ctx_t* pci_ctx = &disk_ctx->pci_device;
    pci_virtio_common_cfg_t* common_cfg;
    
    pci_virtio_capability_t* cap_ptr;
    cap_ptr = virtio_get_capability(pci_ctx, VIRTIO_PCI_CAP_COMMON_CFG);
    ASSERT(cap_ptr != NULL);

    common_cfg = pci_ctx->bar[cap_ptr->bar].vmem + cap_ptr->bar_offset;

    virtio_set_status(common_cfg, VIRTIO_STATUS_ACKNOWLEGE);
    virtio_set_status(common_cfg, VIRTIO_STATUS_DRIVER);

    virtio_set_features(common_cfg, VIRTIO_F_VERSION_1);

    virtio_set_status(common_cfg, VIRTIO_STATUS_FEATURES_OK);
    uint8_t device_status = virtio_get_status(common_cfg);
    ASSERT(device_status & VIRTIO_STATUS_FEATURES_OK);

    virtio_virtq_ctx_t requestq;
    virtio_alloc_queue(common_cfg, VIRTIO_QUEUE_BLOCK_REQUESTQ, 8, 98304, &requestq);
    disk_ctx->virtio_requestq = requestq;

    virtio_set_status(common_cfg, VIRTIO_STATUS_DRIVER_OK);

    pci_virtio_capability_t* blk_cfg_cap;

    blk_cfg_cap = virtio_get_capability(pci_ctx, VIRTIO_PCI_CAP_DEVICE_CFG);
    ASSERT(blk_cfg_cap != NULL);
    disk_ctx->device_config = *((virtio_blk_config_t*)(pci_ctx->bar[blk_cfg_cap->bar].vmem + blk_cfg_cap->bar_offset));
}

static void read_blk_device(blk_disk_ctx_t* disk_ctx, uint64_t sector, void* buffer, uint64_t len) {

    pci_device_ctx_t* pci_ctx = &disk_ctx->pci_device;
    virtio_virtq_buffer_t write_buffers[1] = {0};
    virtio_virtq_buffer_t read_buffers[2] = {0};

    bool get_ok;
    get_ok = virtio_get_buffer(&disk_ctx->virtio_requestq, sizeof(virtio_blk_req_header_t), (uintptr_t*)&write_buffers[0].ptr);
    ASSERT(get_ok);
    write_buffers[0].len = sizeof(virtio_blk_req_header_t);

    virtio_blk_req_header_t* blk_req_header = write_buffers[0].ptr;
    blk_req_header->type = VIRTIO_BLK_T_IN;
    blk_req_header->res = 0;
    blk_req_header->sector = sector;

    get_ok = virtio_get_buffer(&disk_ctx->virtio_requestq, len, (uintptr_t*)&read_buffers[0].ptr);
    ASSERT(get_ok);
    read_buffers[0].len = len;
    get_ok = virtio_get_buffer(&disk_ctx->virtio_requestq, sizeof(virtio_blk_req_status_t), (uintptr_t*)&read_buffers[1].ptr);
    ASSERT(get_ok);
    read_buffers[1].len = sizeof(virtio_blk_req_status_t);

    virtio_virtq_send(&disk_ctx->virtio_requestq, write_buffers, 1,
                      read_buffers, 2);

    virtio_virtq_notify(pci_ctx, &disk_ctx->virtio_requestq);

    virtio_poll_virtq(&disk_ctx->virtio_requestq, true);

    memcpy(buffer, read_buffers[0].ptr, len);

    virtio_return_buffer(&disk_ctx->virtio_requestq, write_buffers[0].ptr);
    virtio_return_buffer(&disk_ctx->virtio_requestq, read_buffers[0].ptr);
    virtio_return_buffer(&disk_ctx->virtio_requestq, read_buffers[1].ptr);
}

static void print_blk_device(pci_device_ctx_t* pci_ctx) {

    pci_header0_t* pci_header = pci_ctx->header_vmem;
    pci_header->command |= 3;

    print_pci_header(pci_ctx);

    print_pci_capabilities(pci_ctx);

    print_blk_config(pci_ctx);
}

static int64_t virtio_pci_blk_open_op(void* ctx, const char* path, const uint64_t flags, void** ctx_out) {
    *ctx_out = ctx;
    return 0;
}

static int64_t virtio_pci_blk_read_op(void* ctx, uint8_t* buffer, const int64_t size, const uint64_t flags) {

    ASSERT(ctx != NULL);
    ASSERT(buffer != NULL);

    blk_disk_ctx_t* blk_ctx = (blk_disk_ctx_t*)ctx;

    uint64_t pos = blk_ctx->device_pos;
    int64_t size_left = size;

    // Prevent reading past the device capacity
    if (blk_ctx->device_config.capacity * 512 < (pos + size_left)) {
        size_left = (blk_ctx->device_config.capacity * 512) - pos;
    }
    int64_t read_size = size_left;

    uint8_t unaligned_buf[512];

    // Handle situations where the current pos is not aligned to a sector
    if ((pos % 512) != 0) {
        uint64_t pos_sector = ALIGN_DOWN(pos, 512) / 512;
        read_blk_device(blk_ctx, pos_sector, unaligned_buf, 512);

        uint64_t valid_idx = pos % 512;
        uint64_t valid_len = 512 - valid_idx;

        if (size_left <= valid_len) {
            memcpy(buffer, &unaligned_buf[valid_idx], size_left);
            blk_ctx->device_pos += read_size;
            return size_left;
        } else {
            memcpy(buffer, &unaligned_buf[valid_idx], valid_len);
            buffer += valid_len;
            size_left -= valid_len;
            pos += valid_len;
        }
    }

    // Pos is now aligned to a sector. Copy the remaining data
    while (size_left > 0) {
        uint64_t pos_sector = pos / 512;
        uint64_t cycle_size = size_left > 16384 ? 16384 : size_left;
        read_blk_device(blk_ctx, pos_sector, buffer, cycle_size);
        size_left -= cycle_size;
        pos += cycle_size;
        buffer += cycle_size;
    }

    blk_ctx->device_pos += read_size;

    return read_size;
}

static int64_t virtio_pci_blk_write_op(void* ctx, const uint8_t* buffer, const int64_t size, const uint64_t flags) {
    return -1;
}

static int64_t virtio_pci_blk_seek_op(void* ctx, const int64_t pos, const uint64_t flags) {
    blk_disk_ctx_t* blk_ctx = (blk_disk_ctx_t*)ctx;

    if (pos > 0) {
        if (pos < (blk_ctx->device_config.capacity * 512)) {
            blk_ctx->device_pos = pos;
            return pos;
        } else {
            return -1;
        }
    }
    return -1;
}

static int64_t virtio_pci_blk_ioctl_op(void* ctx, const uint64_t ioctl, const uint64_t* args, const uint64_t arg_count) {

    blk_disk_ctx_t* blk_ctx = ctx;
    int64_t ret;
    
    switch (ioctl) {
        case IOCTL_SEEK:
            if (arg_count != 2) {
                ret = -1;
            } else {
                ret = virtio_pci_blk_seek_op(ctx, args[0], args[1]);
            }
            break;
        case BLK_IOCTL_SIZE:
            ret = blk_ctx->device_config.capacity;
            break;
        default:
            ret = -1;
    }

    return ret;

}

static fd_ops_t s_bulk_file_ops = {
    .read = virtio_pci_blk_read_op,
    .write = virtio_pci_blk_write_op,
    .ioctl = virtio_pci_blk_ioctl_op,
    .close = NULL
};

static void virtio_pci_blk_ctx(void* ctx_msg) {

    console_write("virtio-pci-blk got ctx\n");
    console_flush();

    discovery_pci_ctx_t* pci_ctx = ctx_msg;

    blk_disk_ctx_t* disk_ctx = vmalloc(sizeof(blk_disk_ctx_t));
    disk_ctx->device_pos = 0;

    pci_alloc_device_from_context(&disk_ctx->pci_device, pci_ctx);

    init_blk_device(disk_ctx);

    print_blk_device(&disk_ctx->pci_device);

    strncpy(disk_ctx->name, "virtio_disk0", MAX_SYS_DEVICE_NAME_LEN);
    disk_ctx->name[11] = '0' + (s_disk_counter % 10);

    sys_device_register(&s_bulk_file_ops, virtio_pci_blk_open_op, disk_ctx, disk_ctx->name);

    llist_append_ptr(s_blk_disks, disk_ctx);

    //uint8_t superblock[1024];
    //read_blk_device(&s_pci_device, 0, superblock, sizeof(superblock));

    //console_printf("Magic: %c\n", superblock[120]);
    //console_flush();
}

static discovery_register_t s_virtio_pci_blk_register = {
    .type = DRIVER_DISCOVERY_PCI,
    .pci = {
        .vendor_id = 0x1AF4,
        .device_id = 0x1042,
    },
    .ctxfunc = virtio_pci_blk_ctx
};

void virtio_pci_blk_register(void) {

    s_blk_disks = llist_create();

    register_driver(&s_virtio_pci_blk_register);
}

REGISTER_DRIVER(virtio_pci_blk);
