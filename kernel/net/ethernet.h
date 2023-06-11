
#ifndef __ETHERNET_H__
#define __ETHERNET_H__

#include <stdint.h>

#include "kernel/net/net.h"

enum {
    NET_ETHERTYPE_ARP = 0x0806,
    NET_ETHERTYPE_IPV4 = 0x0800
};

struct net_dev;
struct net_packet;
struct net_send_buffer;

typedef struct {
    mac_t dest;
    mac_t source;
    uint64_t ethertype;
    uint8_t* payload;
    uint64_t payload_len;
    uint32_t crc32;
} ethernet_l2_frame_t;


int64_t ethernet_parse_l2_frame(struct net_packet* packet, ethernet_l2_frame_t* frame_out);

void ethernet_get_packet_overhead(uint64_t* overhead_out, uint64_t* offset_out);
void ethernet_send_packet(struct net_dev* net_dev, struct net_send_buffer* send_buffer, mac_t* dest_mac, uint16_t ethertype);

#endif
