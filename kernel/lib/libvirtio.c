
#include <stdint.h>

#include "kernel/lib/libpci.h"
#include "kernel/lib/libvirtio.h"
#include "kernel/lib/vmalloc.h"
#include "kernel/interrupt/interrupt.h"
#include "kernel/console.h"
#include "kernel/assert.h"
#include "kernel/kernelspace.h"

#include "stdlib/bitutils.h"

pci_virtio_capability_t* virtio_get_capability(pci_device_ctx_t* device_ctx, uint64_t cap) {

    pci_virtio_capability_t* virtio_capability;
    uint64_t idx = 0;

    do {
        virtio_capability = (pci_virtio_capability_t*)pci_get_capability(device_ctx, PCI_CAP_VENDOR, idx);
        idx++;
    } while (virtio_capability != NULL &&
             virtio_capability->type != cap);
    
    return virtio_capability;
}

uint64_t virtio_get_features(pci_virtio_common_cfg_t* cfg) {

    cfg->device_feature_sel = 1;
    MEM_DMB();
    uint32_t feat_high = cfg->device_feature;
    MEM_DMB();
    cfg->device_feature_sel = 0;
    MEM_DMB();
    uint32_t feat_low = cfg->device_feature;

    return ((uint64_t)feat_high) << 32 | (uint64_t)feat_low;
}

void virtio_set_features(pci_virtio_common_cfg_t* cfg, uint64_t features) {

    uint32_t feat_low = features & 0xFFFFFFFF;
    uint32_t feat_high = (features >> 32) & 0xFFFFFFFF;

    cfg->driver_feature_sel = 0;
    MEM_DMB();
    cfg->driver_feature = feat_low;
    MEM_DMB();
    cfg->driver_feature_sel = 1;
    MEM_DMB();
    cfg->driver_feature = feat_high;
    MEM_DSB();
}

uint8_t virtio_get_status(pci_virtio_common_cfg_t* cfg) {
    return cfg->device_status;
}

void virtio_set_status(pci_virtio_common_cfg_t* cfg, uint8_t status) {
    cfg->device_status |= status;
    MEM_DSB();
}

int64_t virtio_malloc_add_mem(bool initialize, uint64_t req_size, void* ctx, malloc_state_t* state) {

    ASSERT(initialize == true);

    virtio_virtq_ctx_t* queue_ctx = ctx;

    state->base = (uintptr_t)queue_ctx->buffer_pool;
    state->limit = state->base + queue_ctx->buffer_pool_size;

    return queue_ctx->buffer_pool_size;
}

void virtio_alloc_queue(pci_virtio_common_cfg_t* cfg,
                        uint64_t queue_num, uint64_t queue_size,
                        uint64_t pool_size,
                        virtio_virtq_ctx_t* queue_out,
                        uint64_t msix_idx) {

    ASSERT(cfg != NULL);
    ASSERT(queue_out != NULL);

    queue_out->queue_num = queue_num;
    queue_out->queue_size = queue_size;

    // TODO: This should probably be device memory
    queue_out->desc_ptr = vmalloc(16 * queue_size);
    queue_out->desc_phy = KSPACE_TO_PHY(queue_out->desc_ptr);
    ASSERT(queue_out->desc_ptr != NULL);

    queue_out->avail_ptr = vmalloc(6 + 2 * queue_size);
    queue_out->avail_phy = KSPACE_TO_PHY(queue_out->avail_ptr);
    ASSERT(queue_out->avail_ptr != NULL);

    queue_out->used_ptr = vmalloc(6 + 8 * queue_size);
    queue_out->used_phy = KSPACE_TO_PHY(queue_out->used_ptr);
    ASSERT(queue_out->used_ptr != NULL);

    queue_out->avail_ptr->idx = 0;
    queue_out->used_ptr->idx = 0;
    queue_out->last_used_idx = 0;

    queue_out->buffer_pool = vmalloc(pool_size);
    queue_out->buffer_pool_phy = KSPACE_TO_PHY(queue_out->buffer_pool);
    queue_out->buffer_pool_size = pool_size;

    malloc_init_p(&queue_out->buffer_malloc_state, virtio_malloc_add_mem, queue_out);

    cfg->queue_select = queue_num;
    MEM_DSB();
    queue_out->queue_notify_off = cfg->queue_notify_off;

    cfg->queue_size = queue_size;
    cfg->queue_desc = queue_out->desc_phy;
    cfg->queue_driver = queue_out->avail_phy;
    cfg->queue_device = queue_out->used_phy;
    cfg->queue_msix_vector = msix_idx;
    cfg->queue_enable = 1;
    MEM_DSB();
}

bool virtio_get_buffer(virtio_virtq_ctx_t* queue_ctx, uint64_t buffer_len, uintptr_t* buffer_out) {

    ASSERT(queue_ctx != NULL);
    ASSERT(buffer_out != NULL);

    void* buffer_ptr;
    buffer_ptr = malloc_p(buffer_len, &queue_ctx->buffer_malloc_state);
    if (buffer_ptr == NULL) {
        return false;
    }

    *buffer_out = (uintptr_t)buffer_ptr;
    return true;
}

void virtio_return_buffer(virtio_virtq_ctx_t* queue_ctx, void* buffer_ptr) {

    ASSERT(queue_ctx != NULL);
    ASSERT(buffer_ptr != NULL);

    free_p(buffer_ptr, &queue_ctx->buffer_malloc_state);
}

bool virtio_virtq_send(virtio_virtq_ctx_t* queue_ctx,
                       virtio_virtq_buffer_t* write_buffers,
                       uint64_t num_write_buffers,
                       virtio_virtq_buffer_t* read_buffers,
                       uint64_t num_read_buffers) {

    ASSERT(queue_ctx != NULL);
    ASSERT(num_write_buffers + num_read_buffers > 0);

    if (num_write_buffers + num_read_buffers > queue_ctx->queue_size) {
        return false;
    }

    // Write the descriptors
    virtio_virtq_desc_t* virtq_desc = queue_ctx->desc_ptr;

    uint64_t desc_idx = 0;

    for (uint64_t write_idx = 0; write_idx < num_write_buffers; write_idx++) {
        void* write_buffer = write_buffers[write_idx].ptr;

        uintptr_t write_buffer_phy = queue_ctx->buffer_pool_phy + 
                                        (write_buffer - queue_ctx->buffer_pool);
        virtq_desc[desc_idx].addr = write_buffer_phy;
        virtq_desc[desc_idx].len = write_buffers[write_idx].len;
        virtq_desc[desc_idx].flags = VIRTQ_DESC_F_NEXT;
        virtq_desc[desc_idx].next = desc_idx + 1;
        desc_idx++;
    }
    for (uint64_t read_idx = 0; read_idx < num_read_buffers; read_idx++) {
        void* read_buffer = read_buffers[read_idx].ptr;

        uintptr_t read_buffer_phy = queue_ctx->buffer_pool_phy + 
                                        (read_buffer - queue_ctx->buffer_pool);
        virtq_desc[desc_idx].addr = read_buffer_phy;
        virtq_desc[desc_idx].len = read_buffers[read_idx].len;
        virtq_desc[desc_idx].flags = VIRTQ_DESC_F_NEXT |
                                     VIRTQ_DESC_F_WRITE;
        virtq_desc[desc_idx].next = desc_idx + 1;
        desc_idx++;
    }

    virtq_desc[desc_idx-1].flags &= ~(VIRTQ_DESC_F_NEXT);

    MEM_DSB();

    // Write the available ring to start the transaction
    volatile virtio_virtq_avail_t* virtq_avail = queue_ctx->avail_ptr;

    virtq_avail->flags = 0;
    virtq_avail->ring[virtq_avail->idx % queue_ctx->queue_size] = 0;
    MEM_DSB();

    // Pass the buffers to the driver
    virtq_avail->idx += 1;

    MEM_DSB();
    return true;
}

bool virtio_poll_virtq(virtio_virtq_ctx_t* queue_ctx, bool block) {

    MEM_DMB();

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
    volatile uint16_t* used_idx_ptr = &queue_ctx->used_ptr->idx;
#pragma GCC diagnostic pop

    do {
        if (*used_idx_ptr != queue_ctx->last_used_idx) {
            queue_ctx->last_used_idx = *used_idx_ptr;
            MEM_DMB();
            return true;
        }
        MEM_DSB();
        // TODO: Flush cache if not device memory
    } while (block);

    return false;
}

bool virtio_poll_virtq_irq(virtio_virtq_ctx_t* queue_ctx) {

    MEM_DMB();

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
    volatile uint16_t* used_idx_ptr = &queue_ctx->used_ptr->idx;
#pragma GCC diagnostic pop

    do {
        interrupt_await_reset(queue_ctx->intid);
        if (*used_idx_ptr != queue_ctx->last_used_idx) {
            queue_ctx->last_used_idx = *used_idx_ptr;
            MEM_DMB();
            return true;
        }
        interrupt_await(queue_ctx->intid);
        MEM_DSB();
        // TODO: Flush cache if not device memory
    } while (true);

    return false;
}

int64_t virtio_get_used_elem(virtio_virtq_ctx_t* queue_ctx, int64_t desc_idx) {
    for (int idx = 0; idx < queue_ctx->queue_size; idx++) {
        if (queue_ctx->used_ptr->ring[idx].id == desc_idx) {
            return queue_ctx->used_ptr->ring[idx].len;
        }
    }
    ASSERT(false);

    return -1;
}

void virtio_virtq_notify(pci_device_ctx_t* ctx, virtio_virtq_ctx_t* queue_ctx) {
    void* cap_ptr = virtio_get_capability(ctx, VIRTIO_PCI_CAP_NOTIFY_CFG);
    ASSERT(cap_ptr != NULL);

    pci_virtio_notify_capability_t* notify_cap = cap_ptr;

    uint16_t* notify_addr = ctx->bar[notify_cap->cap.bar].vmem + notify_cap->cap.bar_offset +
                                (queue_ctx->queue_notify_off * notify_cap->notify_off_multiplier);
    
    *notify_addr = queue_ctx->queue_num;
    MEM_DSB();
}

void print_pci_capability_qemu(pci_device_ctx_t* device_ctx, pci_vendor_capability_t* cap_ptr) {

    const char* qemu_type_names[] = {
        "Unknown",
        "Virtio PCI Cap Common Cfg",
        "Virtio PCI Cap Notify Cfg",
        "Virito PCI Cap ISR Cfg",
        "Virito PCI Cap Device Cfg",
        "Virito PCI Cap PCI Cfg",
    };

    pci_virtio_capability_t* qemu_cap_ptr = (pci_virtio_capability_t*) cap_ptr;

    console_printf("  QEMU\n");
    if (qemu_cap_ptr->type < VIRTIO_PCI_CAP_MAX) {
        console_printf("  %s\n", qemu_type_names[qemu_cap_ptr->type]);
        console_printf("  BAR [%u] %8x %8x\n",
                       qemu_cap_ptr->bar,
                       qemu_cap_ptr->bar_offset,
                       qemu_cap_ptr->bar_len);

        switch (qemu_cap_ptr->type) {
            case VIRTIO_PCI_CAP_COMMON_CFG:
                print_qemu_capability_common(device_ctx, qemu_cap_ptr);
                break;
        }
    } else {
        console_printf("  Invalid Type %u\n", qemu_cap_ptr->type);
    }

    console_flush();
}

void print_qemu_capability_common(pci_device_ctx_t* device_ctx, pci_virtio_capability_t* cap_ptr) {

    pci_virtio_common_cfg_t* common_cfg;

    common_cfg = device_ctx->bar[cap_ptr->bar].vmem + cap_ptr->bar_offset;
    common_cfg->device_feature_sel = 1;
    uint32_t feat_high = common_cfg->device_feature;
    console_printf("   Device Features: %8x", feat_high);
    common_cfg->device_feature_sel = 0;
    uint32_t feat_low = common_cfg->device_feature;
    console_printf(" %8x\n", feat_low);

    uint64_t device_id = ((pci_header_t*)device_ctx->header_vmem)->device_id - 0x1040;
    print_virtio_feature_bits(feat_low, feat_high, device_id);

    common_cfg->driver_feature_sel = 1;
    console_printf("   Driver Features: %8x", common_cfg->driver_feature);
    common_cfg->driver_feature_sel = 0;
    console_printf(" %8x\n", common_cfg->driver_feature);

    console_printf("   MSI-X Vector: %u\n", common_cfg->msix_config);
    console_printf("   Num Queues: %u\n", common_cfg->num_queues);
    console_printf("   Device Status: %2x\n", common_cfg->device_status);

    console_flush();

    for (int idx = 0; idx < common_cfg->num_queues; idx++) {
        common_cfg->queue_select = idx;
        console_printf("   Queue %u\n", idx);
        console_printf("    Size: %x\n", common_cfg->queue_size);
        console_printf("    MSI-X vector: %x\n", common_cfg->queue_msix_vector);
        console_printf("    Enable: %x\n", common_cfg->queue_enable);
        console_printf("    Notify Offset: %x\n", common_cfg->queue_notify_off);
        console_printf("    Desc: %x\n", common_cfg->queue_desc);
        console_printf("    Driver: %x\n", common_cfg->queue_driver);
        console_printf("    Device: %x\n", common_cfg->queue_device);
        console_flush();
    }

    console_flush();
}

struct virtio_flag_names_t {
    int64_t num;
    char* name;
};

static const struct virtio_flag_names_t virtio_res_flag_names[] = {
    {VIRTIO_F_NOTIFY_ON_EMPTY, "VIRTIO_F_NOTIFY_ON_EMPTY"},
    {VIRTIO_F_ANY_LAYOUT, "VIRTIO_F_ANY_LAYOUT"},
    {VIRTIO_F_RING_INDIRECT_DESC, "VIRTIO_F_RING_INDIRECT_DESC"},
    {VIRTIO_F_RING_EVENT_IDX, "VIRTIO_F_RING_EVENT_IDX"},
    {VIRTIO_F_VERSION_1, "VIRTIO_F_VERSION_1"},
    {VIRTIO_F_ACCESS_PLATFORM, "VIRTIO_F_ACCESS_PLATFORM"},
    {VIRTIO_F_RING_PACKED, "VIRTIO_F_RING_PACKED"},
    {VIRTIO_F_IN_ORDER, "VIRTIO_F_IN_ORDER"},
    {VIRTIO_F_ORDER_PLATFORM, "VIRTIO_F_ORDER_PLATFORM"},
    {VIRTIO_F_SR_IOV, "VIRTIO_F_SR_IOV"},
    {VIRTIO_F_NOTIFICATION_DATA, "VIRTIO_F_NOTIFICATION_DATA"},
    {-1, NULL}
};

static const struct virtio_flag_names_t virtio_blk_flag_names[] = {
    {VIRTIO_BLK_F_SIZE_MAX, "VIRTIO_BLK_F_SIZE_MAX"},
    {VIRTIO_BLK_F_SEG_MAX, "VIRTIO_BLK_F_SEG_MAX"},
    {VIRTIO_BLK_F_GEOMETRY, "VIRTIO_BLK_F_GEOMETRY"},
    {VIRTIO_BLK_F_RO, "VIRTIO_BLK_F_RO"},
    {VIRTIO_BLK_F_BLK_SIZE, "VIRTIO_BLK_F_BLK_SIZE"},
    {VIRTIO_BLK_F_FLUSH, "VIRTIO_BLK_F_FLUSH"},
    {VIRTIO_BLK_F_TOPOLOGY, "VIRTIO_BLK_F_TOPOLOGY"},
    {VIRTIO_BLK_F_CONFIG_WCE, "VIRTIO_BLK_F_CONFIG_WCE"},
    {VIRTIO_BLK_F_DISCARD, "VIRTIO_BLK_F_DISCARD"},
    {VIRTIO_BLK_F_WRITE_ZEROES, "VIRTIO_BLK_F_WRITE_ZEROES"},
    {-1, NULL}
};

static const struct virtio_flag_names_t* virtio_device_flag_names[] = {
    NULL,
    NULL,
    virtio_blk_flag_names
};

void print_virtio_feature_bits(uint32_t feat_low, uint32_t feat_high, uint64_t device_id) {

    uint64_t feat = (((uint64_t)feat_high) << 32) | (uint64_t)feat_low;

    for (int64_t idx = 0; idx < 64; idx++) {
        if (feat & (1UL << idx)) {
            bool found = false;
            for (uint64_t name_idx = 0; name_idx < ARR_ELEMENTS(virtio_res_flag_names); name_idx++) {
                if (virtio_res_flag_names[name_idx].num == idx) {
                    console_printf("   %s\n", virtio_res_flag_names[name_idx].name);
                    found = true;
                }
            }
            if (!found) {

                if (device_id < ARR_ELEMENTS(virtio_device_flag_names)) {
                    const struct virtio_flag_names_t* device_flag_names = virtio_device_flag_names[device_id];
                    if (device_flag_names != NULL) {
                        for (uint64_t name_idx = 0; device_flag_names[name_idx].num != -1; name_idx++) {
                            if (device_flag_names[name_idx].num == idx) {
                                console_printf("   %s\n", device_flag_names[name_idx].name);
                                found = true;
                            }
                        }
                    }
                }
                if (!found) {
                    console_printf("   Unknown Flag: %u\n", idx);
                }
            }
        }
    }
    console_flush();
}

bool virtio_init_with_features(pci_device_ctx_t* pci_ctx, uint64_t features_req) {

    pci_virtio_common_cfg_t* common_cfg = NULL;
    pci_virtio_capability_t* cap_ptr = NULL;

    cap_ptr = virtio_get_capability(pci_ctx, VIRTIO_PCI_CAP_COMMON_CFG);
    ASSERT(cap_ptr);

    common_cfg = GET_CAP_PTR(pci_ctx, cap_ptr);

    virtio_set_status(common_cfg, VIRTIO_STATUS_ACKNOWLEGE);
    virtio_set_status(common_cfg, VIRTIO_STATUS_DRIVER);

    // Check for expected feature bits
    uint64_t features = virtio_get_features(common_cfg);

    if ((features & features_req) != features_req) {
        return false;
    }

    virtio_set_features(common_cfg, features_req);

    virtio_set_status(common_cfg, VIRTIO_STATUS_FEATURES_OK);
    uint8_t device_status = virtio_get_status(common_cfg);

    if (!(device_status & VIRTIO_STATUS_FEATURES_OK)) {
        return false;
    }

    return true;
}