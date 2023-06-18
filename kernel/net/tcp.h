
#ifndef __NET_TCP_H__
#define __NET_TCP_H__

#include <stdint.h>

#include "kernel/net/net.h"
#include "kernel/net/ipv4.h"

#define NET_TCP_HEADER_LEN 20

typedef struct {
    uint16_t source_port;
    uint16_t dest_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t doff:4;
    uint8_t f_cwr:1;
    uint8_t f_ece:1;
    uint8_t f_urg:1;
    uint8_t f_ack:1;
    uint8_t f_psh:1;
    uint8_t f_rst:1;
    uint8_t f_syn:1;
    uint8_t f_fin:1;
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_pointer;

    const void* payload;
    uint64_t payload_len;
} net_tcp_hdr_t;

void net_tcp_send_packet(ipv4_t* dest_ip, net_tcp_hdr_t* tcp_header);
void net_tcp_update_checksum(uint8_t* tcp_payload, uint64_t pseudo_header_checksum);

void net_tcp_handle_packet(net_packet_t* packet, ethernet_l2_frame_t* frame, net_ipv4_hdr_t* ipv4_header);

void net_tcp_print_packet(ipv4_t* source_addr, ipv4_t* dest_addr, net_tcp_hdr_t* tcp_header);


#endif
