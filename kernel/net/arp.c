
#include <stdint.h>
#include <string.h>

#include "kernel/console.h"
#include "kernel/assert.h"
#include "kernel/lib/vmalloc.h"

#include "kernel/net/net.h"
#include "kernel/net/arp.h"
#include "kernel/net/ethernet.h"

#include "stdlib/bitutils.h"

void net_arp_send_packet(net_dev_t* net_dev, net_arp_packet_t* arp_packet, mac_t* dest_mac) {

    uint64_t arp_packet_size = NET_ARP_HDR_PKT_SIZE +
                               arp_packet->hlen * 2 +
                               arp_packet->plen * 2;

    uint64_t ethernet_overhead, ethernet_offset;
    
    ethernet_get_packet_overhead(&ethernet_overhead, &ethernet_offset);

    net_send_buffer_t* send_buffer;
    send_buffer = net_dev->ops->get_buffer(net_dev, arp_packet_size + ethernet_overhead, 0);

    if (send_buffer == NULL) {
        return;
    }

    uint8_t* arp_data = send_buffer->data + ethernet_offset;
    uint16_t tmp;

    tmp = en_swap_16(arp_packet->htype);
    memcpy(&arp_data[0], &tmp, sizeof(uint16_t));
    tmp = en_swap_16(arp_packet->ptype);
    memcpy(&arp_data[2], &tmp, sizeof(uint16_t));
    arp_data[4] = arp_packet->hlen;
    arp_data[5] = arp_packet->plen;
    tmp = en_swap_16(arp_packet->oper);
    memcpy(&arp_data[6], &tmp, sizeof(uint16_t));

    uint64_t arp_offset = 0;
    memcpy(&arp_data[8 + arp_offset], &arp_packet->raw_addrs[arp_offset], arp_packet->hlen);
    arp_offset += arp_packet->hlen;

    memcpy(&arp_data[8 + arp_offset], &arp_packet->raw_addrs[arp_offset], arp_packet->plen);
    arp_offset += arp_packet->plen;

    memcpy(&arp_data[8 + arp_offset], &arp_packet->raw_addrs[arp_offset], arp_packet->hlen);
    arp_offset += arp_packet->hlen;

    memcpy(&arp_data[8 + arp_offset], &arp_packet->raw_addrs[arp_offset], arp_packet->plen);
    arp_offset += arp_packet->plen;

    ethernet_send_packet(net_dev, send_buffer, dest_mac, NET_ETHERTYPE_ARP);
}

static void net_arp_ipv4_respond(net_dev_t* net_dev, net_arp_packet_t* req_packet) {

    net_arp_packet_t reply_packet = {
        .htype = NET_ARP_HTYPE_ETHERNET,
        .ptype = NET_ETHERTYPE_IPV4,
        .hlen = 6,
        .plen = 4,
        .oper = NET_ARP_OP_REPLY
    };

    memcpy(&reply_packet.ipv4.sha, &net_dev->mac, sizeof(mac_t));
    memcpy(&reply_packet.ipv4.spa, &net_dev->ipv4, sizeof(ipv4_t));
    memcpy(&reply_packet.ipv4.tha, &req_packet->ipv4.sha, sizeof(mac_t));
    memcpy(&reply_packet.ipv4.tpa, &req_packet->ipv4.spa, sizeof(ipv4_t));

    net_arp_send_packet(net_dev, &reply_packet, &req_packet->ipv4.sha);
}

static int64_t net_arp_parse_packet(net_packet_t* packet, ethernet_l2_frame_t* frame, net_arp_packet_t* arp_out) {

    uint16_t tmp;
    memcpy(&tmp, &frame->payload[0], sizeof(uint16_t));
    arp_out->htype = en_swap_16(tmp);
    memcpy(&tmp, &frame->payload[2], sizeof(uint16_t));
    arp_out->ptype = en_swap_16(tmp);
    arp_out->hlen = frame->payload[4];
    arp_out->plen = frame->payload[5];
    memcpy(&tmp, &frame->payload[6], sizeof(uint16_t));
    arp_out->oper = en_swap_16(tmp);

    // console_log(LOG_DEBUG, "ARP:");
    // console_log(LOG_DEBUG, " htype: %4x", arp_out->htype);
    // console_log(LOG_DEBUG, " ptype: %4x", arp_out->ptype);
    // console_log(LOG_DEBUG, " hlen: %4x", arp_out->hlen);
    // console_log(LOG_DEBUG, " plen: %4x", arp_out->plen);
    // console_log(LOG_DEBUG, " oper: %4x", arp_out->oper);

    if (arp_out->htype == NET_ARP_HTYPE_ETHERNET &&
        arp_out->ptype == NET_ETHERTYPE_IPV4) {

        if (arp_out->hlen != 6 &&
            arp_out->plen != 4) {
            return -1;
        }

        memcpy(&arp_out->ipv4.sha, &frame->payload[8], sizeof(mac_t));
        memcpy(&arp_out->ipv4.spa, &frame->payload[14], sizeof(mac_t));
        memcpy(&arp_out->ipv4.tha, &frame->payload[18], sizeof(mac_t));
        memcpy(&arp_out->ipv4.tpa, &frame->payload[24], sizeof(mac_t));

        return 0;
    }

    return -1;
}

static void net_arp_handle_ipv4_reply(net_packet_t* packet, net_arp_packet_t* arp_packet) {

    if (memcmp(&packet->dev->mac, &arp_packet->ipv4.tha, sizeof(mac_t)) != 0) {
        return;
    }

    net_arp_update_table(&arp_packet->ipv4.spa, &arp_packet->ipv4.sha);
}

static void net_arp_handle_ipv4_packet(net_packet_t* packet, ethernet_l2_frame_t* frame, net_arp_packet_t* arp_packet) {

    if (arp_packet->oper != NET_ARP_OP_REQUEST) {
        net_arp_handle_ipv4_reply(packet, arp_packet);
        return;
    }

    // console_log(LOG_DEBUG, " Req IP: %d.%d.%d.%d",
    //             LOG_IPV4_ADDR(arp_packet->ipv4.tpa));
    // console_log(LOG_DEBUG, " Our IP: %d.%d.%d.%d",
    //             LOG_IPV4_ADDR(packet->dev->ipv4));

    if (memcmp(&arp_packet->ipv4.tpa, &packet->dev->ipv4, sizeof(ipv4_t)) != 0) {
        return;
    }

    // Found a request for our hw address
    // console_log(LOG_INFO, "Net Arp got request for our hardware address");

    net_arp_ipv4_respond(packet->dev, arp_packet);
}

static void net_arp_handle_packet(net_packet_t* packet, ethernet_l2_frame_t* frame, net_arp_packet_t* arp_packet) {

    if (arp_packet->htype == NET_ARP_HTYPE_ETHERNET &&
        arp_packet->ptype == NET_ETHERTYPE_IPV4 &&
        arp_packet->hlen == 6 &&
        arp_packet->plen == 4) {

        net_arp_handle_ipv4_packet(packet, frame, arp_packet);
        return;
    }

    // Unknown packets should be filtered out in parse_packet
    ASSERT(0);
}

void net_arp_l2_packet_handler(net_packet_t* packet, ethernet_l2_frame_t* frame) {

    if (packet->len < sizeof(net_arp_packet_t)) {
        // console_log(LOG_WARN, "Net Arp invalid packet length %d", packet->len);
        return;
    }

    net_arp_packet_t arp_packet;
    int64_t parse_ok;
    parse_ok = net_arp_parse_packet(packet, frame, &arp_packet);
    if (parse_ok != 0) {
        return;
    }

    net_arp_handle_packet(packet, frame, &arp_packet);
}

void net_arp_init() {

    net_register_l2_handler(0x0806, net_arp_l2_packet_handler);
}