
#include <stdint.h>

#include "kernel/console.h"
#include "kernel/assert.h"
#include "kernel/task.h"
#include "kernel/lib/vmalloc.h"
#include "kernel/lib/intmap.h"
#include "kernel/lib/hashmap.h"
#include "kernel/lib/llist.h"

#include "kernel/net/net.h"
#include "kernel/net/arp.h"
#include "kernel/net/ipv4.h"
#include "kernel/net/ipv4_icmp.h"
#include "kernel/net/ipv4_route.h"
#include "kernel/net/udp.h"
#include "kernel/net/ethernet.h"

#include "include/k_net_api.h"

#include "stdlib/bitutils.h"
#include "stdlib/string.h"

typedef struct {
    net_udp_hdr_t udp_msg;
    ipv4_t sender_ip;
} net_udp_socket_packet_t;

typedef struct {
    task_t* task;

    ipv4_t dest_ip;
    uint16_t source_port;
    uint16_t dest_port;

    llist_head_t incoming_packets;
} net_udp_socket_ctx_t;

hashmap_ctx_t* s_udp_source_port_map = NULL;

void net_udp_socket_recv_packet(net_packet_t* packet, net_ipv4_hdr_t* ipv4_header, net_udp_hdr_t* udp_msg) {

    net_udp_socket_ctx_t* port_ctx;
    uint64_t dest_port64 = udp_msg->dest_port;
    port_ctx = hashmap_get(s_udp_source_port_map, &dest_port64);

    if (port_ctx == NULL) {
        console_log(LOG_DEBUG, "No listener for UDP port %d", dest_port64);
        return;
    }

    net_udp_socket_packet_t* socket_packet = vmalloc(sizeof(net_udp_socket_packet_t));
    uint8_t* payload_buffer = vmalloc(udp_msg->payload_len);
    memcpy(payload_buffer, udp_msg->payload, udp_msg->payload_len);

    socket_packet->udp_msg = *udp_msg;
    socket_packet->udp_msg.payload = payload_buffer;

    socket_packet->sender_ip = ipv4_header->src_ip;

    llist_append_ptr(port_ctx->incoming_packets, socket_packet);
}

static void net_udp_free_socket_packet(net_udp_socket_packet_t* socket_packet) {
    vfree(socket_packet->udp_msg.payload);
    vfree(socket_packet);
}

static int64_t net_udp_socket_read_fn(void* ctx, uint8_t* buffer, const int64_t size, const uint64_t flags) {
    net_udp_socket_ctx_t* socket_ctx = ctx;

    if (!llist_empty(socket_ctx->incoming_packets)) {
        net_udp_socket_packet_t* packet;
        packet = llist_at(socket_ctx->incoming_packets, 0);

        if (size < packet->udp_msg.payload_len) {
            return -1;
        }
        int64_t read_size = packet->udp_msg.payload_len;

        memcpy(buffer, packet->udp_msg.payload, read_size);

        net_udp_free_socket_packet(packet);
        llist_delete_ptr(socket_ctx->incoming_packets, packet);

        return read_size;
    } else {
        if (flags & (1 << K_SOCKET_READ_FLAGS_NONBLOCKING)) {
            return 0;
        } else {
            // TODO: Block here
            ASSERT(0);
            return 0;
        }
    }

}

static int64_t net_udp_socket_write_fn(void* ctx, const uint8_t* buffer, const int64_t size, const uint64_t flags) {
    net_udp_socket_ctx_t* socket_ctx = ctx;

    net_udp_send_packet(&socket_ctx->dest_ip,
                        socket_ctx->dest_port,
                        socket_ctx->source_port,
                        buffer, size);
                    
    return size;
}

static int64_t net_udp_socket_ioctl_fn(void* ctx, const uint64_t ioctl, const uint64_t* args, const uint64_t arg_count) {
    net_udp_socket_ctx_t* socket_ctx = ctx;

    (void)socket_ctx;
    return -1;
}

static int64_t net_udp_socket_close_fn(void* ctx) {
    net_udp_socket_ctx_t* socket_ctx = ctx;

    uint64_t source_port64 = socket_ctx->source_port;

    // net_udp_socket_packet_t* entry;
    // FOR_LLIST(socket_ctx->incoming_packets, entry)
    //     net_udp_free_socket_packet(entry);
    // END_FOR_LLIST()

    //llist_free_all(socket_ctx->incoming_packets);

    void* old_key;
    old_key = hashmap_del(s_udp_source_port_map, &source_port64);
    vfree(old_key);

    vfree(socket_ctx);

    return 0;
}

static const fd_ops_t s_net_udp_socket_ops = {
    .read = net_udp_socket_read_fn,
    .write = net_udp_socket_write_fn,
    .ioctl = net_udp_socket_ioctl_fn,
    .close = net_udp_socket_close_fn
};

int64_t net_udp_create_socket(k_create_socket_t* create_socket_ctx, fd_ops_t* ops, void** ctx_out) {

    if (s_udp_source_port_map == NULL) {
        uint64_t* dummy = vmalloc(sizeof(uint64_t));
        *dummy = 0;
        s_udp_source_port_map = uintmap_alloc(7);
        //hashmap_add(s_udp_source_port_map, dummy, dummy);
    }

    uint64_t source_port64 = create_socket_ctx->udp4.source_port;
    if (source_port64 != 0) {
        if (hashmap_contains(s_udp_source_port_map, &source_port64)) {
            return -1;
        }
    } else {
        for (source_port64 = UDP_EPHIMERAL_START; source_port64 < UINT16_MAX; source_port64++) {
            if (!hashmap_contains(s_udp_source_port_map, &source_port64)) {
                break;
            }
        }

        if (source_port64 == UINT16_MAX) {
            return -1;
        }
    }

    net_udp_socket_ctx_t* socket_ctx = vmalloc(sizeof(net_udp_socket_ctx_t));

    socket_ctx->task = get_active_task();
    memcpy(&socket_ctx->dest_ip, &create_socket_ctx->udp4.dest_ip, sizeof(ipv4_t));
    socket_ctx->source_port = source_port64;
    socket_ctx->dest_port = create_socket_ctx->udp4.dest_port;

    socket_ctx->incoming_packets = llist_create();

    uint64_t* hashmap_key = vmalloc(sizeof(uint64_t));
    *hashmap_key = source_port64;

    hashmap_add(s_udp_source_port_map, hashmap_key, socket_ctx);

    *ops = s_net_udp_socket_ops;
    *ctx_out = socket_ctx;

    return 0;
}


