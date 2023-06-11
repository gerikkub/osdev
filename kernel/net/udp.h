
#ifndef __NET_UDP_H__
#define __NET_UDP_H__

#include <stdint.h>

#include "kernel/net/net.h"
#include "kernel/net/ipv4.h"

typedef struct {
    uint16_t source_port;
    uint16_t dest_port;
    uint16_t len;
    uint16_t checksum;

    void* payload;
    uint64_t payload_len;
} net_udp_hdr_t;

void net_udp_send_packet(ipv4_t* dest_ip, uint64_t dest_port, uint64_t source_port, uint8_t* payload, uint64_t payload_len);
void net_udp_update_checksum(uint8_t* udp_payload, uint64_t pseudo_header_checksum);

void net_udp_handle_packet(net_packet_t* packet, ethernet_l2_frame_t* frame, net_ipv4_hdr_t* ipv4_header);

#endif
