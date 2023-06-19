
#include <stdint.h>

#include "kernel/console.h"
#include "kernel/assert.h"
#include "kernel/lib/vmalloc.h"

#include "kernel/net/net.h"
#include "kernel/net/ipv4.h"
#include "kernel/net/ipv4_icmp.h"
#include "kernel/net/ipv4_route.h"
#include "kernel/net/tcp.h"
#include "kernel/net/tcp_conn.h"
#include "kernel/net/ethernet.h"

#include "stdlib/bitutils.h"
#include "stdlib/string.h"

static uint64_t net_tcp_get_header_len(net_tcp_hdr_t* tcp_header) {
    return tcp_header->doff * 4;
}

void net_tcp_send_packet(ipv4_t* dest_ip, net_tcp_hdr_t* tcp_header) {

    const uint64_t header_len = net_tcp_get_header_len(tcp_header);
    uint64_t tcp_buffer_len = tcp_header->payload_len + header_len;
    uint8_t* tcp_buffer = vmalloc(tcp_buffer_len);

    *(uint16_t*)&tcp_buffer[0] = en_swap_16(tcp_header->source_port);
    *(uint16_t*)&tcp_buffer[2] = en_swap_16(tcp_header->dest_port);
    *(uint32_t*)&tcp_buffer[4] = en_swap_32(tcp_header->seq_num);
    *(uint32_t*)&tcp_buffer[8] = en_swap_32(tcp_header->ack_num);

    tcp_buffer[12] = tcp_header->doff << 4;

    tcp_buffer[13] = tcp_header->f_cwr << 7 |
                     tcp_header->f_ece << 6 |
                     tcp_header->f_urg << 5 |
                     tcp_header->f_ack << 4 |
                     tcp_header->f_psh << 3 |
                     tcp_header->f_rst << 2 |
                     tcp_header->f_syn << 1 |
                     tcp_header->f_fin << 0;

    *(uint16_t*)&tcp_buffer[14] = en_swap_16(tcp_header->window_size);
    *(uint16_t*)&tcp_buffer[16] = 0;
    *(uint16_t*)&tcp_buffer[18] = en_swap_16(tcp_header->urgent_pointer);

    memcpy(&tcp_buffer[20], tcp_header->payload, tcp_header->payload_len);

    uint64_t checksum = 0;
    for (uint64_t idx = 0; idx < tcp_buffer_len/2; idx++) {
        checksum += en_swap_16(*(uint16_t*)&tcp_buffer[idx*2]);
    }
    
    if (tcp_buffer_len % 2 != 0) {
        checksum += ((uint64_t)tcp_buffer[tcp_buffer_len - 1]) << 8;
    }

    checksum = (checksum & 0xFFFF) + (checksum >> 16);
    *(uint16_t*)&tcp_buffer[16] = checksum;

    //console_log(LOG_DEBUG, "Net TCP sending packet");
    //net_tcp_print_packet(NULL, dest_ip, tcp_header);

    if (tcp_header->payload_len > 4) {
        if (memcmp("\x00\x00\x00\x00", tcp_header->payload, 4) == 0) {
            console_log(LOG_DEBUG, "Net TCP sending NULL packet");
        }
    }

    net_ipv4_send_packet(dest_ip, NET_IPV4_PROTO_TCP, tcp_buffer, tcp_buffer_len);

    vfree(tcp_buffer);
}

void net_tcp_update_checksum(uint8_t* tcp_payload, uint64_t pseudo_header_checksum) {

    uint64_t old_checksum = *(uint16_t*)&tcp_payload[16];

    uint64_t new_checksum = old_checksum + pseudo_header_checksum;
    new_checksum = (new_checksum & 0xFFFF) + (new_checksum >> 16);
    new_checksum = new_checksum ^ 0xFFFF;

    if (new_checksum == 0) {
        new_checksum = 0xFFFF;
    }

    *(uint16_t*)&tcp_payload[16] = en_swap_16(new_checksum);
}

static void net_tcp_parse_packet(net_ipv4_hdr_t* ipv4_header, net_tcp_hdr_t* tcp_header) {

    uint8_t* packet = ipv4_header->payload;

    tcp_header->source_port = en_swap_16(*(uint16_t*)&packet[0]);
    tcp_header->dest_port = en_swap_16(*(uint16_t*)&packet[2]);
    tcp_header->seq_num = en_swap_32(*(uint32_t*)&packet[4]);
    tcp_header->ack_num = en_swap_32(*(uint32_t*)&packet[8]);

    tcp_header->doff = (packet[12] >> 4) & 0xF;

    tcp_header->f_cwr = (packet[13] & (1 << 7)) != 0;
    tcp_header->f_ece = (packet[13] & (1 << 6)) != 0;
    tcp_header->f_urg = (packet[13] & (1 << 5)) != 0;
    tcp_header->f_ack = (packet[13] & (1 << 4)) != 0;
    tcp_header->f_psh = (packet[13] & (1 << 3)) != 0;
    tcp_header->f_rst = (packet[13] & (1 << 2)) != 0;
    tcp_header->f_syn = (packet[13] & (1 << 1)) != 0;
    tcp_header->f_fin = (packet[13] & (1 << 0)) != 0;

    tcp_header->window_size = en_swap_16(*(uint16_t*)&packet[14]);
    tcp_header->checksum = en_swap_16(*(uint16_t*)&packet[16]);
    tcp_header->urgent_pointer = en_swap_16(*(uint16_t*)&packet[18]);

    tcp_header->payload = ipv4_header->payload + (tcp_header->doff * 4);
    tcp_header->payload_len = ipv4_header->payload_len - (tcp_header->doff * 4);
}

void net_tcp_handle_packet(net_packet_t* packet, ethernet_l2_frame_t* frame, net_ipv4_hdr_t* ipv4_header) {

    if (ipv4_header->payload_len < NET_TCP_HEADER_LEN) {
        console_log(LOG_WARN, "Net TCP size less than header (%d)", ipv4_header->payload_len);
        return;
    }

    net_tcp_hdr_t tcp_header;
    net_tcp_parse_packet(ipv4_header, &tcp_header);

    net_tcp_handle_connection(packet, ipv4_header, &tcp_header);
}

void net_tcp_print_packet(ipv4_t* source_addr, ipv4_t* dest_addr, net_tcp_hdr_t* tcp_header) {
    console_log(LOG_DEBUG, "Net TCP packet:");
    if (source_addr != NULL) {
        console_log(LOG_DEBUG, " Source: %u.%u.%u.%u:%u Dest: %u.%u.%u.%u:%u",
                    LOG_IPV4_ADDR((*source_addr)), (uint64_t)tcp_header->source_port,
                    LOG_IPV4_ADDR((*dest_addr)), (uint64_t)tcp_header->dest_port);
    } else {
        console_log(LOG_DEBUG, " Dest: %u.%u.%u.%u:%u",
                    LOG_IPV4_ADDR((*dest_addr)), (uint64_t)tcp_header->dest_port);
    }
    console_log(LOG_DEBUG, " Seq: %8x Ack: %8x", (uint64_t)tcp_header->seq_num, (uint64_t)tcp_header->ack_num);
    console_log(LOG_DEBUG, " Doff: %u Flags: %s %s %s %s %s %s %s %s",
                tcp_header->doff,
                tcp_header->f_cwr ? "CWR" : "   ",
                tcp_header->f_ece ? "ECE" : "   ",
                tcp_header->f_urg ? "URG" : "   ",
                tcp_header->f_ack ? "ACK" : "   ",
                tcp_header->f_psh ? "PSH" : "   ",
                tcp_header->f_rst ? "RST" : "   ",
                tcp_header->f_syn ? "SYN" : "   ",
                tcp_header->f_fin ? "FIN" : "   ");
    console_log(LOG_DEBUG, " Window size: %u Checksum: %4x Urgent: %u",
                tcp_header->window_size, tcp_header->checksum,
                tcp_header->urgent_pointer);
    console_log(LOG_DEBUG, " Payload: %16x Length: %u\n",
                tcp_header->payload, tcp_header->payload_len);
}