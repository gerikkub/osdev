
#include <stdint.h>
#include <string.h>

#include "kernel/console.h"
#include "kernel/assert.h"
#include "kernel/lib/vmalloc.h"

#include "kernel/net/net.h"
#include "kernel/net/arp.h"
#include "kernel/net/ipv4.h"
#include "kernel/net/ipv4_icmp.h"
#include "kernel/net/ipv4_route.h"
#include "kernel/net/udp.h"
#include "kernel/net/udp_socket.h"
#include "kernel/net/ethernet.h"

#include "stdlib/bitutils.h"

int64_t net_udp_send_packet(ipv4_t* dest_ip, uint64_t dest_port, uint64_t source_port, const uint8_t* payload, uint64_t payload_len) {

    const uint64_t udp_header_len = 8;

    net_udp_hdr_t udp_header = {
        .source_port = source_port,
        .dest_port = dest_port,
        .len = udp_header_len + payload_len,
        .checksum = 0,
        .payload = payload,
        .payload_len = payload_len
    };


    uint8_t* udp_buffer = vmalloc(udp_header.len);

    *(uint16_t*)&udp_buffer[0] = en_swap_16(udp_header.source_port);
    *(uint16_t*)&udp_buffer[2] = en_swap_16(udp_header.dest_port);
    *(uint16_t*)&udp_buffer[4] = en_swap_16(udp_header.len);
    *(uint16_t*)&udp_buffer[6] = en_swap_16(udp_header.checksum);

    memcpy(&udp_buffer[8], udp_header.payload, udp_header.payload_len);

    uint64_t checksum = 0;

    for (uint64_t idx = 0; idx < udp_header.len/2; idx++) {
        uint16_t* tmp = (uint16_t*)&udp_buffer[idx*2];
        checksum += en_swap_16(*tmp);
    }

    if (udp_header.len % 2 != 0) {
        checksum += ((uint64_t)udp_buffer[udp_header.len - 1]) << 8;
    }

    checksum = (checksum & 0xFFFF) + (checksum >> 16);

    *(uint16_t*)&udp_buffer[6] = (uint16_t)checksum;

    int64_t ip_ret;
    ip_ret = net_ipv4_send_packet(dest_ip, NET_IPV4_PROTO_UDP, udp_buffer, udp_header.len);

    vfree(udp_buffer);

    return ip_ret;
}

void net_udp_update_checksum(uint8_t* udp_payload, uint64_t pseudo_header_checksum) {

    uint64_t old_checksum = *(uint16_t*)&udp_payload[6];

    uint64_t new_checksum = old_checksum + pseudo_header_checksum;
    new_checksum = (new_checksum & 0xFFFF) + (new_checksum >> 16);
    new_checksum = new_checksum ^ 0xFFFF;

    if (new_checksum == 0) {
        new_checksum = 0xFFFF;
    }

    // *(uint16_t*)&udp_payload[6] = en_swap_16(new_checksum);
    *(uint16_t*)&udp_payload[6] = 0;
}

void net_udp_handle_packet(net_packet_t* packet, ethernet_l2_frame_t* frame, net_ipv4_hdr_t* ipv4_header) {
    
    net_udp_hdr_t udp_header;
    if (ipv4_header->payload_len < 8) {
        return;
    }

    udp_header.source_port = en_swap_16(*(uint16_t*)&ipv4_header->payload[0]);
    udp_header.dest_port = en_swap_16(*(uint16_t*)&ipv4_header->payload[2]);
    udp_header.len = en_swap_16(*(uint16_t*)&ipv4_header->payload[4]);
    udp_header.checksum = en_swap_16(*(uint16_t*)&ipv4_header->payload[6]);

    udp_header.payload = ipv4_header->payload + 8;
    udp_header.payload_len = udp_header.len - 8;

    net_udp_socket_recv_packet(packet, ipv4_header, &udp_header);
}
