
#include <stdint.h>

#include "kernel/lib/libpci.h"
#include "kernel/lib/libvirtio.h"
#include "kernel/lib/vmalloc.h"
#include "kernel/interrupt/interrupt.h"
#include "kernel/console.h"
#include "kernel/assert.h"
#include "kernel/task.h"
#include "kernel/kernelspace.h"

#include "stdlib/bitutils.h"
#include "stdlib/printf.h"

pci_cap_t* virtio_get_capability(pci_device_ctx_t* device_ctx, uint8_t cap_type) {

    pci_cap_t* cap;
    uint64_t idx = 0;

    do {
        cap = pci_get_capability(device_ctx, PCI_CAP_VENDOR, idx);
        idx++;
    } while (cap != NULL && PCI_READCAP8_DEV(device_ctx, cap, VIRTIO_PCI_CAP_TYPE) != cap_type);
    
    return cap;
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

uint64_t virtio_poll_virtq_delta(virtio_virtq_ctx_t* queue_ctx) {

    MEM_DMB();

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
    volatile uint16_t* used_idx_ptr = &queue_ctx->used_ptr->idx;
#pragma GCC diagnostic pop

    uint16_t used_idx = *used_idx_ptr;
    uint16_t used_delta = used_idx - queue_ctx->last_used_idx;
    if (used_delta) {
        queue_ctx->last_used_idx = *used_idx_ptr;
    }
    MEM_DMB();
    return used_delta;
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

void virtio_add_irq_to_ctx(virtio_virtq_ctx_t* queue_ctx, virtio_virtq_shared_irq_ctx_t* irq_ctx) {
    queue_ctx->should_wakeup = false;

    if (llist_len(irq_ctx->wait_queue) == 0) {
        interrupt_await_reset(irq_ctx->intid);
    }

    llist_append_ptr(irq_ctx->wait_queue, queue_ctx);
}

bool virtio_wakeup_irq(task_t* task, bool timeout, int64_t* ret) {
    ASSERT(!timeout);
    virtio_virtq_ctx_t* queue_ctx = task->wait_ctx.virtioirq.ctx;

    *ret = 0;
    return queue_ctx->should_wakeup;
}

void virtio_wait_irq(virtio_virtq_ctx_t* queue_ctx, virtio_virtq_shared_irq_ctx_t* irq_ctx) {

    wait_ctx_t wait_ctx = {
        .virtioirq = {
            .ctx = irq_ctx
        },
        .wake_at = 0
    };

    if (llist_at(irq_ctx->wait_queue, 0) == queue_ctx) {
        // Do an irq wait
        interrupt_await(irq_ctx->intid);
    } else {
        // Wait for a signal from the irq handler
        task_wait_kernel(get_active_task(), WAIT_VIRTIOIRQ, &wait_ctx, TASK_WAIT_WAKEUP, virtio_wakeup_irq);
    }

}

uint64_t virtio_poll_virtq_irq(virtio_virtq_ctx_t* queue_ctx, virtio_virtq_shared_irq_ctx_t* irq_ctx) {

    do {
        uint64_t crit_ctx;
        BEGIN_CRITICAL(crit_ctx);

        uint64_t done = virtio_poll_virtq_delta(queue_ctx);
        //bool done = virtio_poll_virtq(queue_ctx, false);
        if (done) {
            END_CRITICAL(crit_ctx);
            return 1;
        }
        virtio_add_irq_to_ctx(queue_ctx, irq_ctx);
        END_CRITICAL(crit_ctx);

        virtio_wait_irq(queue_ctx, irq_ctx);
    } while (true);

    return 0;
}

void virtio_handle_irq(virtio_virtq_shared_irq_ctx_t* irq_ctx) {
    
    virtio_virtq_ctx_t* queue_ctx;
    FOR_LLIST(irq_ctx->wait_queue, queue_ctx)
        queue_ctx->should_wakeup = true;
    END_FOR_LLIST()

    llist_free_all(irq_ctx->wait_queue);
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

int64_t virtio_get_last_used_elem(virtio_virtq_ctx_t* queue_ctx) {

    uint32_t last_idx = (queue_ctx->used_ptr->idx +  (queue_ctx->queue_size - 1)) % queue_ctx->queue_size;

    return queue_ctx->used_ptr->ring[last_idx].len;
}

void virtio_virtq_notify(pci_device_ctx_t* ctx, virtio_virtq_ctx_t* queue_ctx) {
    pci_cap_t* cap = virtio_get_capability(ctx, VIRTIO_PCI_CAP_NOTIFY_CFG);
    ASSERT(cap != 0);

    const uint8_t bar_num = PCI_READCAP8_DEV(ctx, cap, VIRTIO_PCI_CAP_BAR);
    const uint32_t bar_off = PCI_READCAP32_DEV(ctx, cap, VIRTIO_PCI_CAP_BAR_OFF);
    const uint32_t not_off_mul = PCI_READCAP32_DEV(ctx, cap, VIRTIO_PCI_CAP_NOTIFY_OFF_MUL);

    uint16_t* notify_addr = ctx->bar[bar_num].vmem + bar_off +
                                (queue_ctx->queue_notify_off * not_off_mul);
    
    *notify_addr = queue_ctx->queue_num;
    MEM_DSB();
}

void print_pci_capability_virtio(pci_device_ctx_t* device_ctx, pci_cap_t* cap) {

    const char* qemu_type_names[] = {
        "Unknown",
        "Virtio PCI Cap Common Cfg",
        "Virtio PCI Cap Notify Cfg",
        "Virito PCI Cap ISR Cfg",
        "Virito PCI Cap Device Cfg",
        "Virito PCI Cap PCI Cfg",
    };

    const uint8_t cap_type = PCI_READCAP8_DEV(device_ctx, cap, VIRTIO_PCI_CAP_TYPE);

    console_printf("  QEMU\n");
    if (cap_type < VIRTIO_PCI_CAP_MAX) {
        console_printf("  %s\n", qemu_type_names[cap_type]);
        console_printf("  BAR [%u] %8x %8x\n",
                       PCI_READCAP8_DEV(device_ctx, cap, VIRTIO_PCI_CAP_BAR),
                       PCI_READCAP32_DEV(device_ctx, cap, VIRTIO_PCI_CAP_BAR_OFF),
                       PCI_READCAP32_DEV(device_ctx, cap, VIRTIO_PCI_CAP_BAR_LEN));

    } else {
        console_printf("  Invalid Type %u\n", cap_type);
    }

    console_flush();
}

void print_virtio_capability_common(pci_device_ctx_t* device_ctx, pci_cap_t* cap) {

    /*
    pci_virtio_common_cfg_t* common_cfg;
    const uint8_t bar_num = VIRTIO_PCI_CAP_BAR);
    const uint32_t bar_off = *(uint8_t*)(header_mem + cap_off + VIRTIO_PCI_CAP_BAR_OFF);

    common_cfg = device_ctx->bar[bar_num].vmem + bar_off;
    common_cfg->device_feature_sel = 1;
    uint32_t feat_high = common_cfg->device_feature;
    console_printf("   Device Features: %8x", feat_high);
    common_cfg->device_feature_sel = 0;
    uint32_t feat_low = common_cfg->device_feature;
    console_printf(" %8x\n", feat_low);

    uint64_t device_id = device_ctx->device_id - 0x1040;
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
    */
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

void virtio_init_pci_caps(pci_device_ctx_t* device_ctx) {

    pci_cap_t* cap;
    FOR_LLIST(device_ctx->cap_list, cap)
        if (cap->cap == PCI_CAP_VENDOR) {
            uint8_t bar = PCI_READCAP8_DEV(device_ctx, cap, VIRTIO_PCI_CAP_BAR);
            uint32_t bar_off = PCI_READCAP32_DEV(device_ctx, cap, VIRTIO_PCI_CAP_BAR_OFF);

            cap->ctx = device_ctx->bar[bar].vmem + bar_off;
        }
    END_FOR_LLIST()
}

bool virtio_init_with_features(pci_device_ctx_t* device_ctx, uint64_t features_req) {

    pci_cap_t* cap = virtio_get_capability(device_ctx, VIRTIO_PCI_CAP_COMMON_CFG);
    ASSERT(cap != NULL);
    pci_virtio_common_cfg_t* common_cfg = cap->ctx;

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
