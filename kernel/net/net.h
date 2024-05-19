
#ifndef __NET_H__
#define __NET_H__

#include "kernel/net/nic_ops.h"

#define NET_MTU 1514

#define LOG_IPV4_ADDR(x) (uint64_t)(x).d[0], (uint64_t)(x).d[1], (uint64_t)(x).d[2], (uint64_t)(x).d[3]

typedef struct {
    uint8_t d[6];
} mac_t;

typedef struct {
    uint8_t d[4];
} ipv4_t;

#include "kernel/net/ethernet.h"

typedef struct net_dev {
    nic_ops_t* ops;
    void* nic_ctx;

    char* name;

    mac_t mac;
    ipv4_t ipv4;

} net_dev_t;

typedef struct net_packet {
    net_dev_t* dev;

    uint8_t* data;
    uint64_t len;

    void* nic_pkt_ctx;
} net_packet_t;

typedef struct net_send_buffer {
    net_dev_t* dev;

    uint8_t* data;
    uint64_t len;

    void* nic_buffer_ctx;
} net_send_buffer_t;

typedef void (*net_l2_packet_fn)(net_packet_t* packet, ethernet_l2_frame_t* frame);

void net_recv_packet(net_packet_t* packet);

void net_device_register(net_dev_t* dev);

void net_register_l2_handler(uint64_t ethertype, net_l2_packet_fn handler);

#endif
