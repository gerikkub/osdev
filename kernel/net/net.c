
#include <stdint.h>
#include <string.h>

#include "kernel/assert.h"
#include "kernel/console.h"
#include "kernel/lib/llist.h"
#include "kernel/lib/vmalloc.h"
#include "kernel/lib/intmap.h"
#include "kernel/lib/hashmap.h"
#include "kernel/drivers.h"
#include "kernel/fd.h"
#include "kernel/sys_device.h"
#include "kernel/kernelspace.h"
#include "kernel/memoryspace.h"

#include "kernel/net/net.h"
#include "kernel/net/nic_ops.h"
#include "kernel/net/ethernet.h"
#include "kernel/net/ipv4.h"
#include "kernel/net/ipv4_route.h"

#include "stdlib/bitutils.h"

#include "include/k_ioctl_common.h"

int64_t net_fd_read_op(void* ctx, uint8_t* buffer, const int64_t size, const uint64_t flags) {
    return -1;
}

int64_t net_fd_write_op(void* ctx, const uint8_t* vuffer, const int64_t size, const uint64_t flags) {
    return -1;
}

int64_t net_fd_ioctl_op(void* ctx, const uint64_t ioctl, const uint64_t* args, const uint64_t arg_count) {
    net_dev_t* nic_ctx = ctx;
    uint32_t temp32;
    
    switch (ioctl) {
        case NET_IOCTL_SET_IP:
            if (arg_count != 1) {
                return -1;
            }

            nic_ctx->ipv4.d[3] = (*args) & 0xFF;
            nic_ctx->ipv4.d[2] = ((*args) >> 8) & 0xFF;
            nic_ctx->ipv4.d[1] = ((*args) >> 16) & 0xFF;
            nic_ctx->ipv4.d[0] = ((*args) >> 24) & 0xFF;
            console_log(LOG_DEBUG, "Set IP for %s to %d.%d.%d.%d",
                        nic_ctx->name,
                        LOG_IPV4_ADDR(nic_ctx->ipv4));
            return 0;
        case NET_IOCTL_GET_IP:
            if (arg_count != 0) {
                return -1;
            }

            int64_t ret = (int64_t)nic_ctx->ipv4.d[3] |
                          ((int64_t)nic_ctx->ipv4.d[2] << 8) |
                          ((int64_t)nic_ctx->ipv4.d[1] << 16) |
                          ((int64_t)nic_ctx->ipv4.d[0] << 24);
            return ret;
        case NET_IOCTL_SET_ROUTE:
            if (arg_count != 2) {
                return -1;
            }

            temp32 = en_swap_32(args[0]);
            net_route_update_entry((ipv4_t*)&temp32, args[1], nic_ctx);
            return 0;
        case NET_IOCTL_SET_DEFAULT_ROUTE:
            if (arg_count != 2) {
                return -1;
            }

            temp32 = en_swap_32(args[0]);
            net_route_update_default_entry((ipv4_t*)&temp32, args[1], nic_ctx);
            return 0;
    }

    return nic_ctx->ops->ioctl(nic_ctx, ioctl, args, arg_count);
}

int64_t net_fd_close_op(void* ctx) {
    return 0;
}

int64_t net_fd_open_op(void* ctx, const char* path, const uint64_t flags, void** ctx_out, fd_ctx_t* fd_ctx) {
    *ctx_out = ctx;
    return 0;
}

fd_ops_t s_net_fd_ops = {
    .read = net_fd_read_op,
    .write = net_fd_write_op,
    .ioctl = net_fd_ioctl_op,
    .close = net_fd_close_op
};

hashmap_ctx_t* s_ethertype_handlers = NULL;

void net_recv_packet(net_packet_t* packet) {

    int64_t res;
    ethernet_l2_frame_t* frame_ptr = vmalloc(sizeof(ethernet_l2_frame_t));
    res = ethernet_parse_l2_frame(packet, frame_ptr);

    if (res != 0) {
        packet->dev->ops->return_packet(packet);
        vfree(frame_ptr);
        return;
    }

    if (memcmp(&packet->dev->mac, &frame_ptr->dest, sizeof(mac_t)) != 0 && 
        memcmp("\xff\xff\xff\xff\xff\xff", &frame_ptr->dest, sizeof(mac_t)) != 0) {
        packet->dev->ops->return_packet(packet);
        vfree(frame_ptr);
        return;
    }

    if (memcmp("\xff\xff\xff\xff\xff\xff", &frame_ptr->dest, sizeof(mac_t)) == 0) {
        console_log(LOG_DEBUG, "Got a broadcast packet!");
    } else {
        console_log(LOG_DEBUG, "Got a packet for us!");
    }

    ASSERT(s_ethertype_handlers);

    uint64_t ethertype = frame_ptr->ethertype;

    console_log(LOG_DEBUG, "Ethertype: %4x", ethertype);

    net_l2_packet_fn l2_packet_handler = hashmap_get(s_ethertype_handlers, &ethertype);
    
    if (l2_packet_handler == NULL) {
        packet->dev->ops->return_packet(packet);
        vfree(frame_ptr);
        return;
    }

    l2_packet_handler(packet, frame_ptr);

    vfree(frame_ptr);
    packet->dev->ops->return_packet(packet);
}

void net_device_register(net_dev_t* dev) {

    sys_device_register(&s_net_fd_ops, net_fd_open_op, dev, dev->name);

    console_log(LOG_INFO, "Created NIC %s with MAC %2x:%2x:%2x:%2x:%2x:%2x",
                dev->name,
                dev->mac.d[0],
                dev->mac.d[1],
                dev->mac.d[2],
                dev->mac.d[3],
                dev->mac.d[4],
                dev->mac.d[5]);
}

void net_register_l2_handler(uint64_t ethertype, net_l2_packet_fn handler) {

    ASSERT(ethertype <= UINT16_MAX);

    if (s_ethertype_handlers == NULL) {
        s_ethertype_handlers = uintmap_alloc(16);
        ASSERT(s_ethertype_handlers);
    }

    ASSERT(!hashmap_contains(s_ethertype_handlers, &ethertype));

    uint64_t* key = vmalloc(sizeof(uint64_t));
    // TODO: This will crash if the item is ever removed because
    // hashmap_del will call vfree on handler

    *key = ethertype;
    hashmap_add(s_ethertype_handlers, key, handler);

    console_log(LOG_DEBUG, "Net created l2 handler for %u", ethertype);
}
