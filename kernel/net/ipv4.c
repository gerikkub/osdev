
#include <stdint.h>

#include "kernel/console.h"
#include "kernel/assert.h"
#include "kernel/lib/vmalloc.h"

#include "kernel/net/net.h"
#include "kernel/net/arp.h"
#include "kernel/net/ipv4.h"
#include "kernel/net/ipv4_icmp.h"
#include "kernel/net/ipv4_route.h"
#include "kernel/net/udp.h"
#include "kernel/net/ethernet.h"

#include "stdlib/bitutils.h"
#include "stdlib/string.h"

void net_ipv4_send_packet(ipv4_t* dest_ip, uint16_t protocol, void* payload, uint64_t payload_len) {

    net_dev_t* net_dev = NULL;
    net_route_get_nic_for_ipv4(dest_ip, &net_dev);

    if (net_dev == NULL) {
        console_log(LOG_WARN, "Net IPv4 No Known Route to %d.%d.%d.%d",
                    dest_ip->d[0], dest_ip->d[1], dest_ip->d[2], dest_ip->d[3]);
        return;
    }

    bool arp_ok;
    mac_t dest_mac;
    arp_ok = net_arp_get_mac_for_ipv4(net_dev, dest_ip, &dest_mac);

    if (!arp_ok) {
        return;
    }

    // Support fragmentation later
    ASSERT(payload_len < 1400);

    static uint16_t ipv4_id_counter = 0;

    net_ipv4_hdr_t ipv4_header = {
        .version = 4,
        .ihl = 5,
        .dscp = 0,
        .ecn = 0,
        .total_len = NET_IPV4_HEADER_LEN + payload_len,
        .id = ipv4_id_counter,
        .flags = 0,
        .fragment_offset = 0,
        .ttl = 128,
        .protocol = protocol
    };

    ipv4_id_counter++;

    ipv4_header.checksum = 0;
    memcpy(&ipv4_header.src_ip, &net_dev->ipv4, sizeof(ipv4_t));
    memcpy(&ipv4_header.dst_ip, dest_ip, sizeof(ipv4_t));

    ipv4_header.payload = payload;
    ipv4_header.payload_len = payload_len;

    uint64_t eth_overhead, eth_offset;
    ethernet_get_packet_overhead(&eth_overhead, &eth_offset);

    net_send_buffer_t* send_buffer;
    send_buffer = net_dev->ops->get_buffer(net_dev, ipv4_header.total_len + eth_overhead, 0);

    if (send_buffer == NULL) {
        return;
    }

    uint8_t* ipv4_payload = send_buffer->data + eth_offset;

    ipv4_payload[0] = ((ipv4_header.version << 4) & 0xF0) | (ipv4_header.ihl & 0xF);
    ipv4_payload[1] = ((ipv4_header.dscp << 2) & 0xFC) | (ipv4_header.ecn & 3);
    *(uint16_t*)&ipv4_payload[2] = en_swap_16(ipv4_header.total_len);
    *(uint16_t*)&ipv4_payload[4] = en_swap_16(ipv4_header.id);
    *(uint16_t*)&ipv4_payload[6] = en_swap_16(((ipv4_header.flags << 13) & 0xE0) | (ipv4_header.fragment_offset & 0x1FFF));
    ipv4_payload[8] = ipv4_header.ttl;
    ipv4_payload[9] = ipv4_header.protocol;
    *(uint16_t*)&ipv4_payload[10] = en_swap_16(ipv4_header.checksum);
    memcpy(&ipv4_payload[12], &ipv4_header.src_ip, sizeof(ipv4_t));
    memcpy(&ipv4_payload[16], &ipv4_header.dst_ip, sizeof(ipv4_t));

    memcpy(&ipv4_payload[20], payload, payload_len);

    uint64_t checksum = 0;
    switch (protocol) {
        case NET_IPV4_PROTO_UDP:


            // Source and Destination IP
            for (uint64_t idx = 0; idx < 4; idx++) {
                uint16_t* ipv4_u16 = (uint16_t*)&ipv4_payload[12 + idx*2];
                checksum += en_swap_16(*ipv4_u16);
            }

            checksum += ipv4_header.protocol;

            checksum += (uint16_t)payload_len;

            net_udp_update_checksum(&ipv4_payload[20], checksum);
            break;
    }

    checksum = 0;
    for (uint64_t idx = 0; idx < 10; idx++) {
        uint16_t* ipv4_u16 = (uint16_t*)&ipv4_payload[idx*2];
        checksum += *ipv4_u16;
    }

    ipv4_header.checksum = (uint16_t)~((checksum & 0xFFFF) + (checksum >> 16));
    *(uint16_t*)&ipv4_payload[10] = ipv4_header.checksum;

    ethernet_send_packet(net_dev, send_buffer, &dest_mac, NET_ETHERTYPE_IPV4);
}

int64_t net_ipv4_parse_packet(net_packet_t* packet, ethernet_l2_frame_t* frame, net_ipv4_hdr_t* ipv4_header) {

    if (frame->payload_len < 20) {
        return -1;
    }

    uint16_t tmp16;
    ipv4_header->version = (frame->payload[0] >> 4) & 0xF;
    ipv4_header->ihl = frame->payload[0] & 0xF;

    ipv4_header->dscp = (frame->payload[1] >> 2) & 0x3F;
    ipv4_header->ecn = frame->payload[1] & 0x3;

    memcpy(&tmp16, &frame->payload[2], sizeof(uint16_t));
    ipv4_header->total_len = en_swap_16(tmp16);

    memcpy(&tmp16, &frame->payload[4], sizeof(uint16_t));
    ipv4_header->id = en_swap_16(tmp16);

    memcpy(&tmp16, &frame->payload[6], sizeof(uint16_t));
    ipv4_header->fragment_offset = en_swap_16(tmp16) & 0x1FFF;
    ipv4_header->flags = (en_swap_16(tmp16) >> 13) & 0x7;

    ipv4_header->ttl = frame->payload[8];

    ipv4_header->protocol = frame->payload[9];

    memcpy(&tmp16, &frame->payload[10], sizeof(uint16_t));
    ipv4_header->checksum = en_swap_16(tmp16);

    memcpy(&ipv4_header->src_ip, &frame->payload[12], sizeof(ipv4_t));
    memcpy(&ipv4_header->dst_ip, &frame->payload[16], sizeof(ipv4_t));

    ipv4_header->payload = frame->payload + ipv4_header->ihl * 4;
    ipv4_header->payload_len = frame->payload_len - ipv4_header->ihl * 4;

    //TODO: Handle options

    return 0;
}

void net_ipv4_l2_packet_handler(net_packet_t* packet, ethernet_l2_frame_t* frame) {

    net_ipv4_hdr_t ipv4_header;
    int64_t parse_ok;
    parse_ok = net_ipv4_parse_packet(packet, frame, &ipv4_header);
    
    if (parse_ok != 0) {
        return;
    }

    // Drop the message if it's not our IP
    if (memcmp(&ipv4_header.dst_ip, &packet->dev->ipv4, sizeof(ipv4_t)) != 0) {
        return;
    }

    switch (ipv4_header.protocol) {
        case NET_IPV4_PROTO_ICMP:
            net_ipv4_handle_icmp(packet, frame, &ipv4_header);
            break;
        case NET_IPV4_PROTO_UDP:
            net_udp_handle_packet(packet, frame, &ipv4_header);
            break;
    }
}

void net_ipv4_init() {

    net_register_l2_handler(0x0800, net_ipv4_l2_packet_handler);
}

