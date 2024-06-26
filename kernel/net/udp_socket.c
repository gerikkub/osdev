
#include <stdint.h>
#include <string.h>

#include "kernel/console.h"
#include "kernel/assert.h"
#include "kernel/task.h"
#include "kernel/select.h"
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
#include "kernel/net/udp_socket.h"
#include "kernel/net/ethernet.h"

#include "include/k_ioctl_common.h"
#include "include/k_net_api.h"
#include "include/k_select.h"

#include "stdlib/bitutils.h"

typedef struct {
    net_udp_hdr_t udp_msg;
    ipv4_t sender_ip;
} net_udp_socket_packet_t;

typedef struct {
    ipv4_t dest_ip;
    uint16_t source_port;
    uint16_t dest_port;

    llist_head_t incoming_packets;

    fd_ctx_t* fd_ctx;
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

    port_ctx->fd_ctx->ready |= FD_READY_GEN_READ;
    select_task_wakeup(port_ctx->fd_ctx->task);
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

        if (llist_empty(socket_ctx->incoming_packets)) {
            socket_ctx->fd_ctx->ready &= ~FD_READY_GEN_READ;
        }

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

    int64_t udp_ok;
    udp_ok = net_udp_send_packet(&socket_ctx->dest_ip,
                                 socket_ctx->dest_port,
                                 socket_ctx->source_port,
                                 buffer, size);
                    
    return udp_ok == 0 ? size : udp_ok;
}

static int64_t net_udp_socket_ioctl_fn(void* ctx, const uint64_t ioctl, const uint64_t* args, const uint64_t arg_count) {
    net_udp_socket_ctx_t* socket_ctx = ctx;

    uint64_t info_raw_ptr;
    switch (ioctl) {
        case SOCKET_IOCTL_GET_INFO:
            if (arg_count != 1) {
                return -1;
            }
            info_raw_ptr = args[0];
            k_socket_info_t* socket_info = get_kptr_for_ptr(info_raw_ptr);
            if (socket_info == NULL) {
                return -1;
            }

            socket_info->socket_type = SYSCALL_SOCKET_UDP4;
            *(ipv4_t*)&socket_info->udp4.dest_ip = socket_ctx->dest_ip;
            socket_info->udp4.source_port = socket_ctx->source_port;
            socket_info->udp4.dest_port = socket_ctx->dest_port;

            return 0;
        case SOCKET_IOCTL_GET_MSGINFO:
            if (arg_count != 1) {
                console_log(LOG_INFO, "Bad argcount %d", arg_count);
                return -1;
            }
            if (llist_empty(socket_ctx->incoming_packets)) {
                console_log(LOG_INFO, "No packets");
                return -1;
            }
            info_raw_ptr = args[0];
            k_socket_msginfo_t* msg_info = get_kptr_for_ptr(info_raw_ptr);
            if (msg_info == NULL) {
                console_log(LOG_INFO, "Bad ptr %16x %16x", info_raw_ptr, msg_info);
                return -1;
            }

            net_udp_socket_packet_t* packet;
            packet = llist_at(socket_ctx->incoming_packets, 0);

            msg_info->socket_type = SYSCALL_SOCKET_UDP4;
            msg_info->len = packet->udp_msg.len;
            msg_info->udp4.source_ip = *(k_ipv4_t*)&packet->sender_ip;
            msg_info->udp4.source_port = packet->udp_msg.source_port;

            return 0;
        case SOCKET_IOCTL_SET_CONFIG:
            if (arg_count != 1) {
                console_log(LOG_INFO, "Bad argcount %d", arg_count);
                return -1;
            }
            info_raw_ptr = args[0];
            k_socket_config_t* socket_cfg = get_kptr_for_ptr(info_raw_ptr);
            if (socket_cfg == NULL) {
                console_log(LOG_INFO, "Bad ptr %16x %16x", info_raw_ptr, socket_cfg);
                return -1;
            }

            if (socket_cfg->socket_type != SYSCALL_SOCKET_UDP4) {
                console_log(LOG_INFO, "Bad socket_type, %d (%16x)", socket_cfg->socket_type, socket_cfg);
                return -1;
            }

            if (socket_cfg->udp4.flags & K_SOCKET_CONFIG_DEST_IP) {
                socket_ctx->dest_ip = *(ipv4_t*)&socket_cfg->udp4.dest_ip;
            }
            if (socket_cfg->udp4.flags & K_SOCKET_CONFIG_DEST_PORT) {
                socket_ctx->dest_port = socket_cfg->udp4.dest_port;
            }

            return 0;
        default:
            return -1;
    }

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

int64_t net_udp_create_socket(k_create_socket_t* create_socket_ctx) {

    task_t* task = get_active_task();
    int64_t fd_num = find_open_fd(task);
    if (fd_num < 0) {
        return -1;
    }
    fd_ctx_t* fd_ctx = get_task_fd(fd_num, task);

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

    memcpy(&socket_ctx->dest_ip, &create_socket_ctx->udp4.dest_ip, sizeof(ipv4_t));
    socket_ctx->source_port = source_port64;
    socket_ctx->dest_port = create_socket_ctx->udp4.dest_port;
    socket_ctx->fd_ctx = fd_ctx;

    socket_ctx->incoming_packets = llist_create();

    uint64_t* hashmap_key = vmalloc(sizeof(uint64_t));
    *hashmap_key = source_port64;

    hashmap_add(s_udp_source_port_map, hashmap_key, socket_ctx);

    fd_ctx->ops = s_net_udp_socket_ops;
    fd_ctx->ctx = socket_ctx;
    fd_ctx->ready = FD_READY_GEN_WRITE;
    fd_ctx->task = task;
    fd_ctx->valid = true;

    return fd_num;
}


