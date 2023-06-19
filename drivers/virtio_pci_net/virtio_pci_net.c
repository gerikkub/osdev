
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
#include "kernel/kernelspace.h"
#include "kernel/memoryspace.h"
#include "kernel/vmem.h"
#include "kernel/task.h"
#include "kernel/kmalloc.h"

#include "kernel/net/net.h"
#include "kernel/net/nic_ops.h"

#include "include/k_ioctl_common.h"

#include "stdlib/bitutils.h"
#include "stdlib/string.h"

typedef struct {
    pci_device_ctx_t* pci_ctx;
    pci_virtio_capability_t* virtio_cfg_cap;

    virtio_virtq_ctx_t receiveq1;
    virtio_virtq_ctx_t transmitq1;

    virtio_virtq_shared_irq_ctx_t receiveq1_irq_ctx;

    net_dev_t net_dev;

} virtio_pci_net_ctx_t;

static void virtio_pci_net_device_irq_fn(uint32_t intid, void* ctx) {
    virtio_pci_net_ctx_t* net_ctx = ctx;
    pci_interrupt_clear_pending(net_ctx->pci_ctx, intid);

    if (net_ctx->receiveq1_irq_ctx.intid == intid) {
        virtio_handle_irq(&net_ctx->receiveq1_irq_ctx);
    }
}

static void virtio_net_recv_thread(void* ctx) {

    virtio_pci_net_ctx_t* net_ctx = ctx;

    const uint64_t buf_size = NET_MTU + sizeof(virtio_net_hdr_t);

    virtio_virtq_buffer_t* recv_buffer = vmalloc(sizeof(virtio_virtq_buffer_t));
    bool status;
    status = virtio_get_buffer(&net_ctx->receiveq1, buf_size, (uintptr_t*)&recv_buffer->ptr);
    ASSERT(status);
    recv_buffer->len = buf_size;

    virtio_virtq_send(&net_ctx->receiveq1,
                      NULL, 0,
                      recv_buffer, 1);
    virtio_virtq_notify(net_ctx->pci_ctx, &net_ctx->receiveq1);


    while (1) {

        virtio_poll_virtq_irq(&net_ctx->receiveq1, &net_ctx->receiveq1_irq_ctx);

        int64_t recv_len = virtio_get_last_used_elem(&net_ctx->receiveq1);
        ASSERT(recv_len > 0);

        net_packet_t* packet = vmalloc(sizeof(net_packet_t));
        packet->dev = &net_ctx->net_dev;
        packet->data = recv_buffer->ptr + sizeof(virtio_net_hdr_t);
        packet->len = recv_len - sizeof(virtio_net_hdr_t);
        packet->nic_pkt_ctx = recv_buffer;

        recv_buffer = vmalloc(sizeof(virtio_virtq_buffer_t));
        status = virtio_get_buffer(&net_ctx->receiveq1, buf_size, (uintptr_t*)&recv_buffer->ptr);
        ASSERT(status);
        recv_buffer->len = buf_size;

        virtio_virtq_send(&net_ctx->receiveq1,
                        NULL, 0,
                        recv_buffer, 1);
        virtio_virtq_notify(net_ctx->pci_ctx, &net_ctx->receiveq1);

        net_recv_packet(packet);
    }

}

void virtio_pci_net_nic_return_packet(struct net_packet* packet) {
    virtio_virtq_buffer_t* virtq_buffer = packet->nic_pkt_ctx;
    net_dev_t* net_dev = packet->dev;
    virtio_pci_net_ctx_t* nic_ctx = net_dev->nic_ctx;

    console_log(LOG_DEBUG, "Returning packet %16x", packet);

    virtio_return_buffer(&nic_ctx->receiveq1, virtq_buffer->ptr);
    (void)net_dev;
}

int64_t virtio_pci_net_nic_ioctl(void* ctx, const uint64_t ioctl, const uint64_t* args, const uint64_t arg_count) {
    return -1;
}

net_send_buffer_t* virtio_pci_net_nic_get_buffer(net_dev_t* net_dev, const int64_t size, const uint64_t flags) {

    virtio_pci_net_ctx_t* nic_ctx = net_dev->nic_ctx;

    net_send_buffer_t* send_buffer = vmalloc(sizeof(net_send_buffer_t));

    virtio_virtq_buffer_t* virtq_send_buffer = vmalloc(sizeof(virtio_virtq_buffer_t));
    bool get_ok;
    get_ok = virtio_get_buffer(&nic_ctx->transmitq1, size + sizeof(virtio_net_hdr_t), (uintptr_t*)&virtq_send_buffer->ptr);
    
    if (!get_ok) {
        console_log(LOG_WARN, "Net %s unable to allocate a buffer", net_dev->name);
        vfree(send_buffer);
        vfree(virtq_send_buffer);
        return NULL;
    }

    send_buffer->dev = net_dev;
    send_buffer->data = virtq_send_buffer->ptr + sizeof(virtio_net_hdr_t);
    send_buffer->len = size;
    send_buffer->nic_buffer_ctx = virtq_send_buffer;

    return send_buffer;
}

void virtio_pci_net_nic_free_buffer(net_dev_t* net_dev, net_send_buffer_t* send_buffer) {
    virtio_pci_net_ctx_t* nic_ctx = net_dev->nic_ctx;
    virtio_virtq_buffer_t* virtq_send_buffer = send_buffer->nic_buffer_ctx;
    virtio_return_buffer(&nic_ctx->transmitq1, virtq_send_buffer->ptr);
}

void virtio_pci_net_nic_send_buffer(net_dev_t* net_dev, net_send_buffer_t* send_buffer) {

    virtio_pci_net_ctx_t* nic_ctx = net_dev->nic_ctx;
    virtio_virtq_buffer_t* virtq_send_buffer = send_buffer->nic_buffer_ctx;

    virtio_net_hdr_t* send_header = virtq_send_buffer->ptr;

    send_header->flags = 0;
    send_header->gso_type = 0;
    send_header->hdr_len = 0;
    send_header->gso_size = 0;
    send_header->csum_start = 0;
    send_header->csum_offset = 0;
    send_header->num_buffers = 0;

    virtq_send_buffer->len = send_buffer->len + sizeof(virtio_net_hdr_t);

    //console_log(LOG_DEBUG, "Net Transmit packet of size %d", send_buffer->len);

    virtio_virtq_send(&nic_ctx->transmitq1, virtq_send_buffer, 1, NULL, 0);
    virtio_virtq_notify(nic_ctx->pci_ctx, &nic_ctx->transmitq1);

    virtio_poll_virtq(&nic_ctx->transmitq1, true);

    virtio_pci_net_nic_free_buffer(net_dev, send_buffer);
}


static nic_ops_t s_nic_ops = {
    .get_buffer = virtio_pci_net_nic_get_buffer,
    .send_buffer = virtio_pci_net_nic_send_buffer,
    .free_buffer = virtio_pci_net_nic_free_buffer,
    .return_packet = virtio_pci_net_nic_return_packet,
    .ioctl = virtio_pci_net_nic_ioctl
};

static void virtio_pci_net_late_init(void* ctx) {
    discovery_pci_ctx_t* pci_ctx = ctx;

    virtio_pci_net_ctx_t* nic_ctx = vmalloc(sizeof(virtio_pci_net_ctx_t));
    nic_ctx->pci_ctx = vmalloc(sizeof(pci_device_ctx_t));

    pci_alloc_device_from_context(nic_ctx->pci_ctx, pci_ctx);

    pci_virtio_capability_t* cap_ptr = NULL;
    cap_ptr = virtio_get_capability(nic_ctx->pci_ctx, VIRTIO_PCI_CAP_COMMON_CFG);

    uint64_t features_req = (1UL << VIRTIO_NET_F_MAC) |
                            (1UL << VIRTIO_NET_F_STATUS) |
                            (1UL << VIRTIO_F_VERSION_1);
    bool status = virtio_init_with_features(nic_ctx->pci_ctx, features_req);
    ASSERT(status);

    uint32_t receiveq_intid;
    receiveq_intid = pci_register_interrupt_handler(nic_ctx->pci_ctx,
                                                    virtio_pci_net_device_irq_fn,
                                                    nic_ctx);
                                                
    pci_msix_vector_ctx_t* receiveq_msix_item = pci_get_msix_entry(nic_ctx->pci_ctx, receiveq_intid);
    ASSERT(receiveq_msix_item);

    nic_ctx->virtio_cfg_cap = NULL;
    nic_ctx->virtio_cfg_cap = virtio_get_capability(nic_ctx->pci_ctx, VIRTIO_PCI_CAP_DEVICE_CFG);
    ASSERT(nic_ctx->virtio_cfg_cap);

    pci_virtio_common_cfg_t* common_cfg = NULL;
    common_cfg = GET_CAP_PTR(nic_ctx->pci_ctx, cap_ptr);

    virtio_alloc_queue(common_cfg,
                       VIRTIO_QUEUE_NET_RECEIVEQ1,
                       //32, 32 * 16384,
                       4, 4 * 8000,
                       &nic_ctx->receiveq1,
                       receiveq_msix_item->entry_idx);

    virtio_alloc_queue(common_cfg,
                       VIRTIO_QUEUE_NET_TRANSMITQ1,
                       32, 32 * 16384,
                       &nic_ctx->transmitq1, 0);

    nic_ctx->receiveq1_irq_ctx.wait_queue = llist_create();
    nic_ctx->receiveq1_irq_ctx.intid = receiveq_intid;

    virtio_set_status(common_cfg, VIRTIO_STATUS_DRIVER_OK);

    pci_enable_vector(nic_ctx->pci_ctx, nic_ctx->receiveq1_irq_ctx.intid);
    pci_enable_interrupts(nic_ctx->pci_ctx);

    pci_virtio_capability_t* net_cfg_cap = virtio_get_capability(nic_ctx->pci_ctx, VIRTIO_PCI_CAP_DEVICE_CFG); 
    virtio_net_config_t* net_cfg = GET_CAP_PTR(nic_ctx->pci_ctx, net_cfg_cap);

    nic_ctx->net_dev.ops = &s_nic_ops;
    nic_ctx->net_dev.nic_ctx = nic_ctx;
    memcpy(&nic_ctx->net_dev.mac, net_cfg->mac, sizeof(mac_t));
    nic_ctx->net_dev.name = "virtio-pci-net0";

    net_device_register(&nic_ctx->net_dev);

    create_kernel_task(8192, virtio_net_recv_thread, nic_ctx);

    vfree(pci_ctx);
}

static void virtio_pci_net_ctx(void* ctx) {
    console_log(LOG_INFO, "virtio-pci-net found device");

    discovery_pci_ctx_t* pci_ctx = ctx;
    discovery_pci_ctx_t* pci_ctx_copy = vmalloc(sizeof(discovery_pci_ctx_t));
    *pci_ctx_copy = *pci_ctx;

    driver_register_late_init(virtio_pci_net_late_init, pci_ctx_copy);
}

static discovery_register_t s_virtio_pci_net_register = {
    .type = DRIVER_DISCOVERY_PCI,
    .pci = {
        .vendor_id = 0x1af4,
        .device_id = 0x1000,
    },
    .ctxfunc = virtio_pci_net_ctx
};

void virtio_pci_net_register(void) {

    register_driver(&s_virtio_pci_net_register);
}

REGISTER_DRIVER(virtio_pci_net);
