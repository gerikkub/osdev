
#include <stdint.h>
#include <string.h>

#include "kernel/assert.h"
#include "kernel/console.h"
#include "kernel/lib/libpci.h"
#include "kernel/lib/libvirtio.h"
#include "kernel/lib/llist.h"
#include "kernel/lib/vmalloc.h"
#include "kernel/drivers.h"
#include "kernel/fd.h"
#include "kernel/sys_device.h"
#include "kernel/kernelspace.h"
#include "kernel/memoryspace.h"
#include "kernel/vmem.h"
#include "kernel/kmalloc.h"

#include "include/k_ioctl_common.h"

#include "stdlib/bitutils.h"

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
    void* cache_virtaddr;
    virtio_virtq_shared_irq_ctx_t virtio_irq_ctx;
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

/*
static void print_blk_config(pci_device_ctx_t* pci_ctx) {

    pci_virtio_capability_t* blk_cfg_cap;

    blk_cfg_cap = virtio_get_capability(pci_ctx, VIRTIO_PCI_CAP_DEVICE_CFG);
    ASSERT(blk_cfg_cap != NULL);

    virtio_blk_config_t* blk_config;

    blk_config = pci_ctx->bar[blk_cfg_cap->bar].vmem + blk_cfg_cap->bar_offset;

    console_log(LOG_INFO, "Disk Capacity: %u KBs\n", blk_config->capacity * 512 / 1024);
    console_flush();
}
*/

static void virtio_pci_blk_device_irq_fn(uint32_t intid, void* ctx) {
    blk_disk_ctx_t* disk_ctx = ctx;
    pci_interrupt_clear_pending(&disk_ctx->pci_device, intid);

    virtio_handle_irq(&disk_ctx->virtio_irq_ctx);
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

    uint32_t queue_intid;
    queue_intid = pci_register_interrupt_handler(
                                        &disk_ctx->pci_device,
                                        virtio_pci_blk_device_irq_fn,
                                        disk_ctx);

    bool found = false;
    pci_msix_vector_ctx_t* msix_item = NULL;

    FOR_LLIST(disk_ctx->pci_device.msix_vector_list, msix_item)
        if (msix_item->intid == queue_intid) {
            found = true;
            break;
        }
    END_FOR_LLIST()

    ASSERT(found);

    console_log(LOG_DEBUG, "virtio_pci_blk: Registered IRQ %d\n", queue_intid);

    disk_ctx->virtio_irq_ctx.wait_queue = llist_create();
    disk_ctx->virtio_irq_ctx.intid = queue_intid;

    virtio_alloc_queue(common_cfg,
                       VIRTIO_QUEUE_BLOCK_REQUESTQ,
                       8, 98304,
                       &disk_ctx->virtio_requestq,
                       msix_item->entry_idx);

    virtio_set_status(common_cfg, VIRTIO_STATUS_DRIVER_OK);

    pci_virtio_capability_t* blk_cfg_cap;

    blk_cfg_cap = virtio_get_capability(pci_ctx, VIRTIO_PCI_CAP_DEVICE_CFG);
    ASSERT(blk_cfg_cap != NULL);
    disk_ctx->device_config = *((virtio_blk_config_t*)(pci_ctx->bar[blk_cfg_cap->bar].vmem + blk_cfg_cap->bar_offset));

    pci_enable_vector(&disk_ctx->pci_device, queue_intid);
    pci_enable_interrupts(&disk_ctx->pci_device);
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

    uint64_t delta = virtio_poll_virtq_irq(&disk_ctx->virtio_requestq, &disk_ctx->virtio_irq_ctx);
    (void)delta;

    // TODO: Necessary delay for some file reads. Not sure what the underlying problem is
    for (volatile int i = 0; i < 100000; i++);

    memcpy(buffer, read_buffers[0].ptr, len);

    virtio_return_buffer(&disk_ctx->virtio_requestq, write_buffers[0].ptr);
    virtio_return_buffer(&disk_ctx->virtio_requestq, read_buffers[0].ptr);
    virtio_return_buffer(&disk_ctx->virtio_requestq, read_buffers[1].ptr);
}

static void write_blk_device(blk_disk_ctx_t* disk_ctx, uint64_t sector, void* buffer, uint64_t len) {

    pci_device_ctx_t* pci_ctx = &disk_ctx->pci_device;
    virtio_virtq_buffer_t write_buffers[2] = {0};
    virtio_virtq_buffer_t read_buffers[1] = {0};

    bool get_ok;
    get_ok = virtio_get_buffer(&disk_ctx->virtio_requestq, sizeof(virtio_blk_req_header_t), (uintptr_t*)&write_buffers[0].ptr);
    ASSERT(get_ok);
    write_buffers[0].len = sizeof(virtio_blk_req_header_t);

    virtio_blk_req_header_t* blk_req_header = write_buffers[0].ptr;
    blk_req_header->type = VIRTIO_BLK_T_OUT;
    blk_req_header->res = 0;
    blk_req_header->sector = sector;

    get_ok = virtio_get_buffer(&disk_ctx->virtio_requestq, len, (uintptr_t*)&write_buffers[1].ptr);
    ASSERT(get_ok);
    write_buffers[1].len = len;
    memcpy(write_buffers[1].ptr, buffer, len);

    get_ok = virtio_get_buffer(&disk_ctx->virtio_requestq, sizeof(virtio_blk_req_status_t), (uintptr_t*)&read_buffers[0].ptr);
    ASSERT(get_ok);
    read_buffers[0].len = sizeof(virtio_blk_req_status_t);

    virtio_virtq_send(&disk_ctx->virtio_requestq, write_buffers, 2,
                      read_buffers, 1);

    virtio_virtq_notify(pci_ctx, &disk_ctx->virtio_requestq);

    virtio_poll_virtq_irq(&disk_ctx->virtio_requestq, &disk_ctx->virtio_irq_ctx);
    //virtio_poll_virtq(&disk_ctx->virtio_requestq, true);

    virtio_return_buffer(&disk_ctx->virtio_requestq, write_buffers[0].ptr);
    virtio_return_buffer(&disk_ctx->virtio_requestq, write_buffers[1].ptr);
    virtio_return_buffer(&disk_ctx->virtio_requestq, read_buffers[0].ptr);
}

/*
static void print_blk_device(pci_device_ctx_t* pci_ctx) {

    pci_header0_t* pci_header = pci_ctx->header_vmem;
    pci_header->command |= 3;

    print_pci_header(pci_ctx);

    print_pci_capabilities(pci_ctx);

    print_blk_config(pci_ctx);
}
*/

static bool virtio_blk_populate_virt_cache(void* ctx, uintptr_t addr, memcache_phy_entry_t* new_entry) {

    blk_disk_ctx_t* disk_ctx = ctx;

    void* phy_addr = kmalloc_phy(VMEM_PAGE_SIZE);

    uintptr_t addr_page = PAGE_FLOOR(addr);
    ASSERT(addr_page >= (uintptr_t)disk_ctx->cache_virtaddr);
    uint64_t offset = addr_page - (uintptr_t)disk_ctx->cache_virtaddr;
    uint64_t sector = offset / 512;
    ASSERT(sector <= disk_ctx->device_config.capacity);

    read_blk_device(disk_ctx, sector, PHY_TO_KSPACE_PTR(phy_addr), VMEM_PAGE_SIZE);

    new_entry->offset = offset;
    new_entry->phy_addr = (uintptr_t)phy_addr;
    new_entry->len = VMEM_PAGE_SIZE;

    return true;
}

memcache_ops_t s_virtio_blk_memcache_ops = {
    .populate_virt_fn = virtio_blk_populate_virt_cache
};

static int64_t virtio_pci_blk_open_op(void* ctx, const char* path, const uint64_t flags, void** ctx_out, fd_ctx_t* fd_ctx) {
    *ctx_out = ctx;

    if (fd_ctx != NULL) {
        fd_ctx->ready = FD_READY_ALL;
    }

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
    int64_t read_size = size_left < size ? size_left : size;

    // Map all necessary memory
    uint64_t pos_page = PAGE_FLOOR(pos);
    uint64_t read_max = pos + read_size;
    bool updated_vm = false;
    memory_entry_cache_t* mem_entry = NULL;
    while (pos_page < read_max) {
        uint64_t par_el1 = vmem_check_address((uintptr_t)blk_ctx->cache_virtaddr + pos_page);
        if ((par_el1 & 1) == 1) {
            // Translation failed. Read in page
            mem_entry = (memory_entry_cache_t*)memspace_get_entry_at_addr_kernel(blk_ctx->cache_virtaddr);
            ASSERT(mem_entry != NULL && mem_entry->type == MEMSPACE_CACHE);

            memcache_phy_entry_t* new_phy_entry = vmalloc(sizeof(memcache_phy_entry_t));
            bool ok = virtio_blk_populate_virt_cache(blk_ctx, (uintptr_t)blk_ctx->cache_virtaddr + pos_page, new_phy_entry);
            ASSERT(ok);

            llist_append_ptr(mem_entry->phy_addr_list, new_phy_entry);
            updated_vm = true;
        }

        pos_page += VMEM_PAGE_SIZE;
    }

    if (updated_vm) {
        memspace_update_kernel_cache(mem_entry);
        memspace_update_kernel_vmem();
    }

    memcpy(buffer, blk_ctx->cache_virtaddr + pos, read_size);

    blk_ctx->device_pos += read_size;

    return read_size;
}

static int64_t virtio_pci_blk_write_op(void* ctx, const uint8_t* buffer, const int64_t size, const uint64_t flags) {

    ASSERT(ctx != NULL);
    ASSERT(buffer != NULL);

    blk_disk_ctx_t* blk_ctx = (blk_disk_ctx_t*)ctx;

    uint64_t pos = blk_ctx->device_pos;

    int64_t size_left = size;

    // Prevent reading past the device capacity
    if (blk_ctx->device_config.capacity * 512 < (pos + size_left)) {
        size_left = (blk_ctx->device_config.capacity * 512) - pos;
    }
    int64_t write_size = size_left < size ? size_left : size;

    // Map all necessary memory
    uint64_t pos_page = PAGE_FLOOR(pos);
    uint64_t write_max = pos + write_size;
    bool updated_vm = false;
    memory_entry_cache_t* mem_entry = NULL;
    while (pos_page < write_max) {
        uint64_t par_el1 = vmem_check_address((uintptr_t)blk_ctx->cache_virtaddr + pos_page);
        if ((par_el1 & 1) == 1) {
            // Translation failed. Read in page
            mem_entry = (memory_entry_cache_t*)memspace_get_entry_at_addr_kernel(blk_ctx->cache_virtaddr);
            ASSERT(mem_entry != NULL && mem_entry->type == MEMSPACE_CACHE);

            memcache_phy_entry_t* new_phy_entry = vmalloc(sizeof(memcache_phy_entry_t));
            bool ok = virtio_blk_populate_virt_cache(blk_ctx, (uintptr_t)blk_ctx->cache_virtaddr + pos_page, new_phy_entry);
            ASSERT(ok);

            llist_append_ptr(mem_entry->phy_addr_list, new_phy_entry);
            updated_vm = true;
        }

        pos_page += VMEM_PAGE_SIZE;
    }

    if (updated_vm) {
        memspace_update_kernel_cache(mem_entry);
        memspace_update_kernel_vmem();
    }

    memcpy(blk_ctx->cache_virtaddr + pos, buffer, write_size);

    uint64_t end_pos = blk_ctx->device_pos + write_size;

    for (uint64_t write_page = PAGE_FLOOR(pos); write_page < end_pos; write_page += VMEM_PAGE_SIZE) {
        uint64_t write_sector = write_page / 512;
        write_blk_device(blk_ctx, write_sector, blk_ctx->cache_virtaddr + write_page, VMEM_PAGE_SIZE);
    }

    blk_ctx->device_pos += write_size;

    return write_size;
}

static int64_t virtio_pci_blk_seek_op(void* ctx, const int64_t pos, const uint64_t flags) {
    blk_disk_ctx_t* blk_ctx = (blk_disk_ctx_t*)ctx;

    if (pos >= 0) {
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

static void virtio_pci_blk_late_init(void* ctx) {
    discovery_pci_ctx_t* pci_ctx = ctx;

    blk_disk_ctx_t* disk_ctx = vmalloc(sizeof(blk_disk_ctx_t));
    disk_ctx->device_pos = 0;

    pci_alloc_device_from_context(&disk_ctx->pci_device, pci_ctx);

    init_blk_device(disk_ctx);

    strncpy(disk_ctx->name, "virtio_disk0", MAX_SYS_DEVICE_NAME_LEN);
    disk_ctx->name[11] = '0' + (s_disk_counter % 10);

    sys_device_register(&s_bulk_file_ops, virtio_pci_blk_open_op, disk_ctx, disk_ctx->name);

    llist_append_ptr(s_blk_disks, disk_ctx);

    vfree(pci_ctx);

    uint64_t len_page = PAGE_CEIL(disk_ctx->device_config.capacity * 512);
    disk_ctx->cache_virtaddr = memspace_alloc_kernel_virt(len_page, VMEM_PAGE_SIZE);
    memory_entry_cache_t blk_cache_entry = {
        .start = (uintptr_t)disk_ctx->cache_virtaddr,
        .end = (uintptr_t)disk_ctx->cache_virtaddr + len_page,
        .type = MEMSPACE_CACHE,
        .flags = MEMSPACE_FLAG_PERM_KRW,
        .phy_addr_list = NULL,
        .cacheops_ptr = &s_virtio_blk_memcache_ops,
        .cacheops_ctx = disk_ctx,
        .res = {0}
    };
    blk_cache_entry.phy_addr_list = llist_create();
    memspace_add_entry_to_kernel_memory((memory_entry_t*)&blk_cache_entry);

    memspace_update_kernel_vmem();

}


static void virtio_pci_blk_ctx(void* ctx) {

    discovery_pci_ctx_t* pci_ctx = ctx;
    discovery_pci_ctx_t* pci_ctx_copy = vmalloc(sizeof(discovery_pci_ctx_t));
    *pci_ctx_copy = *pci_ctx;

    driver_register_late_init(virtio_pci_blk_late_init, pci_ctx_copy);
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
