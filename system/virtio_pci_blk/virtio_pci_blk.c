
#include <stdint.h>

#include "system/lib/system_lib.h"
#include "system/lib/system_msg.h"
#include "system/lib/system_assert.h"
#include "system/lib/system_console.h"
#include "system/lib/libpci.h"
#include "system/lib/libvirtio.h"

#include "stdlib/printf.h"
#include "stdlib/bitutils.h"
#include "stdlib/string.h"

static pci_device_ctx_t s_pci_device = {0};
static virtio_virtq_ctx_t s_virtio_requestq = {0};

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
    SYS_ASSERT(blk_cfg_cap != NULL);

    virtio_blk_config_t* blk_config;

    blk_config = pci_ctx->bar[blk_cfg_cap->bar].vmem + blk_cfg_cap->bar_offset;

    console_printf("Disk Capacity: %u KBs\n", blk_config->capacity * 512 / 1024);
    console_flush();
}

static void init_blk_device(pci_device_ctx_t* pci_ctx) {

    pci_virtio_common_cfg_t* common_cfg;
    
    pci_virtio_capability_t* cap_ptr;
    cap_ptr = virtio_get_capability(pci_ctx, VIRTIO_PCI_CAP_COMMON_CFG);
    SYS_ASSERT(cap_ptr != NULL);

    common_cfg = pci_ctx->bar[cap_ptr->bar].vmem + cap_ptr->bar_offset;

    virtio_set_status(common_cfg, VIRTIO_STATUS_ACKNOWLEGE);
    virtio_set_status(common_cfg, VIRTIO_STATUS_DRIVER);

    virtio_set_features(common_cfg, VIRTIO_F_VERSION_1);

    virtio_set_status(common_cfg, VIRTIO_STATUS_FEATURES_OK);
    uint8_t device_status = virtio_get_status(common_cfg);
    SYS_ASSERT(device_status & VIRTIO_STATUS_FEATURES_OK);

    virtio_virtq_ctx_t requestq;
    virtio_alloc_queue(common_cfg, VIRTIO_QUEUE_BLOCK_REQUESTQ, 8, 65536, &requestq);
    s_virtio_requestq = requestq;

    virtio_set_status(common_cfg, VIRTIO_STATUS_DRIVER_OK);
}

static void read_blk_device(pci_device_ctx_t* pci_ctx, uint64_t sector, void* buffer, uint64_t len) {

    virtio_virtq_buffer_t write_buffers[1] = {0};
    virtio_virtq_buffer_t read_buffers[2] = {0};

    bool get_ok;
    get_ok = virtio_get_buffer(&s_virtio_requestq, sizeof(virtio_blk_req_header_t), (uintptr_t*)&write_buffers[0].ptr);
    SYS_ASSERT(get_ok);
    write_buffers[0].len = sizeof(virtio_blk_req_header_t);

    virtio_blk_req_header_t* blk_req_header = write_buffers[0].ptr;
    blk_req_header->type = VIRTIO_BLK_T_IN;
    blk_req_header->res = 0;
    blk_req_header->sector = sector;

    get_ok = virtio_get_buffer(&s_virtio_requestq, len, (uintptr_t*)&read_buffers[0].ptr);
    SYS_ASSERT(get_ok);
    read_buffers[0].len = len;
    get_ok = virtio_get_buffer(&s_virtio_requestq, sizeof(virtio_blk_req_status_t), (uintptr_t*)&read_buffers[1].ptr);
    SYS_ASSERT(get_ok);
    read_buffers[1].len = sizeof(virtio_blk_req_status_t);

    virtio_virtq_send(&s_virtio_requestq, write_buffers, 1,
                      read_buffers, 2);

    virtio_virtq_notify(pci_ctx, &s_virtio_requestq);

    virtio_poll_virtq(&s_virtio_requestq, true);

    memcpy(buffer, read_buffers[0].ptr, len);

    //virtio_return_buffer(&s_virtio_requestq, read_buffers[0].ptr);
    //virtio_return_buffer(&s_virtio_requestq, write_buffers[0].ptr);
    //virtio_return_buffer(&s_virtio_requestq, write_buffers[1].ptr);
}

static void print_blk_device(pci_device_ctx_t* pci_ctx) {

    pci_header0_t* pci_header = pci_ctx->header_vmem;
    pci_header->command |= 3;

    print_pci_header(pci_ctx);

    print_pci_capabilities(pci_ctx);

    print_blk_config(pci_ctx);

    /*
    for (int idx = 0; idx < 6; idx++) {
        if (pci_ctx->bar[idx].allocated) {
            uint32_t* vmem = pci_ctx->bar[idx].vmem;
            uint64_t len = pci_ctx->bar[idx].len / 4;
            console_printf("Bar ");
            switch (pci_ctx->bar[idx].space) {
                case PCI_SPACE_IO:
                    console_printf("IO ");
                    break;
                case PCI_SPACE_M32:
                    console_printf("M32 ");
                    break;
                case PCI_SPACE_M64:
                    console_printf("M64 ");
                    break;
                default:
                    SYS_ASSERT(0);
            }
            console_printf("Phy %16x Virt %16x Len %x (%u)\n",
                           pci_ctx->bar[idx].phy,
                           pci_ctx->bar[idx].vmem,
                           len, len);
            console_flush();

            (void)vmem;
        }
    }
    */
}

static void virtio_pci_blk_ctx(system_msg_memory_t* ctx_msg) {

    console_write("virtio-pci-blk got ctx\n");
    console_flush();

    module_pci_ctx_t* pci_ctx = (module_pci_ctx_t*)ctx_msg->ptr;

    pci_alloc_device_from_context(&s_pci_device, pci_ctx);

    init_blk_device(&s_pci_device);

    print_blk_device(&s_pci_device);

    uint8_t superblock[1024];
    read_blk_device(&s_pci_device, 0, superblock, sizeof(superblock));

    console_printf("Magic: %c\n", superblock[120]);
    console_flush();
}

static module_handlers_t s_handlers = {
    { //generic
        .info = NULL,
        .getinfo = NULL,
        .ioctl = NULL,
        .ctx = virtio_pci_blk_ctx
    }
};

void main(uint64_t my_tid, module_ctx_t* ctx) {

    system_register_handler(s_handlers, MOD_CLASS_DISCOVERY);

    console_write("virtio-pci-blk driver\n");
    console_flush();

    while (1) {
        system_recv_msg();
    }
}