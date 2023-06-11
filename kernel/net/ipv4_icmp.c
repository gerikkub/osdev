
#include <stdint.h>

#include "kernel/console.h"
#include "kernel/assert.h"
#include "kernel/lib/vmalloc.h"

#include "kernel/net/net.h"
#include "kernel/net/ipv4.h"
#include "kernel/net/ipv4_icmp.h"
#include "kernel/net/ethernet.h"

#include "stdlib/bitutils.h"
#include "stdlib/string.h"


void net_ipv4_icmp_send_packet(ipv4_t* dest_ip, mac_t* dest_mac, net_ipv4_icmp_hdr_t* icmp_msg) {

    const uint64_t icmp_header_len = 4;
    uint64_t icmp_msg_len;

    switch (icmp_msg->type) {
        case NET_IPV4_ICMP_ECHO_REPLY:
        case NET_IPV4_ICMP_ECHO_REQUEST:
            icmp_msg_len = icmp_header_len + 4 + icmp_msg->payload_len;
            break;
        default:
            icmp_msg_len = icmp_header_len + icmp_msg->payload_len;
            break;
    }

    uint8_t* icmp_send_buffer = vmalloc(icmp_msg_len);

    icmp_send_buffer[0] = icmp_msg->type;
    icmp_send_buffer[1] = icmp_msg->code;
    *(uint16_t*)&icmp_send_buffer[2] = 0;

    switch (icmp_msg->type) {
        case NET_IPV4_ICMP_ECHO_REPLY:
        case NET_IPV4_ICMP_ECHO_REQUEST:
            *(uint16_t*)&icmp_send_buffer[4] = en_swap_16(icmp_msg->echo.id);
            *(uint16_t*)&icmp_send_buffer[6] = en_swap_16(icmp_msg->echo.seq);
            break;
        default:
            break;
    }

    memcpy(&icmp_send_buffer[icmp_msg_len - icmp_msg->payload_len], icmp_msg->payload, icmp_msg->payload_len);

    uint64_t checksum = 0;
    for (uint64_t idx = 0; idx < icmp_msg_len/2; idx++) {
        uint16_t* ipv4_u16 = (uint16_t*)&icmp_send_buffer[idx*2];
        checksum += *ipv4_u16;
    }

    uint16_t checksum16 = (uint16_t)~((checksum & 0xFFFF) + (checksum >> 16));
    *(uint16_t*)&icmp_send_buffer[2] = checksum16;

    net_ipv4_send_packet(dest_ip, dest_mac, NET_IPV4_PROTO_ICMP, icmp_send_buffer, icmp_msg_len);

    vfree(icmp_send_buffer);
}

static void net_ipv4_handle_icmp_echo_reply(net_packet_t* packet, ethernet_l2_frame_t* frame, net_ipv4_hdr_t* ipv4_header, net_ipv4_icmp_hdr_t* icmp_header) {

    console_log(LOG_DEBUG, "Net ICMP recevied echo REPLAY from %d.%d.%d.%d (%d %d)",
                LOG_IPV4_ADDR(ipv4_header->dst_ip),
                icmp_header->echo.id, icmp_header->echo.seq);

}

static void net_ipv4_handle_icmp_echo_request(net_packet_t* packet, ethernet_l2_frame_t* frame, net_ipv4_hdr_t* ipv4_header, net_ipv4_icmp_hdr_t* icmp_header) {

    console_log(LOG_DEBUG, "Net ICMP recevied echo REQUEST from %d.%d.%d.%d (%d %d)",
                LOG_IPV4_ADDR(ipv4_header->dst_ip),
                icmp_header->echo.id, icmp_header->echo.seq);


    net_ipv4_icmp_hdr_t icmp_response = {
        .type = NET_IPV4_ICMP_ECHO_REPLY,
        .code = 0,
        .checksum = 0,
        .echo.id = icmp_header->echo.id,
        .echo.seq = icmp_header->echo.seq,
        .payload = icmp_header->payload,
        .payload_len = icmp_header->payload_len
    };

    net_ipv4_icmp_send_packet(&ipv4_header->src_ip, &frame->source, &icmp_response);
}

void net_ipv4_handle_icmp(net_packet_t* packet, ethernet_l2_frame_t* frame, net_ipv4_hdr_t* ipv4_header) {

    net_ipv4_icmp_hdr_t icmp_header;

    uint16_t tmp16;
    icmp_header.type = ipv4_header->payload[0];
    icmp_header.code = ipv4_header->payload[1];
    memcpy(&tmp16, &ipv4_header->payload[2], sizeof(uint16_t));
    icmp_header.checksum = en_swap_16(tmp16);

    icmp_header.payload = &ipv4_header->payload[4];
    icmp_header.payload_len = ipv4_header->payload_len - 4;


    switch (icmp_header.type) {
        case NET_IPV4_ICMP_ECHO_REPLY:
        case NET_IPV4_ICMP_ECHO_REQUEST:
            memcpy(&tmp16, &icmp_header.payload[0], sizeof(uint16_t));
            icmp_header.echo.id = en_swap_16(tmp16);
            memcpy(&tmp16, &icmp_header.payload[2], sizeof(uint16_t));
            icmp_header.echo.seq = en_swap_16(tmp16);

            icmp_header.payload += 4;
            icmp_header.payload_len -= 4;
            break;
        default:
            break;
    }

    switch (icmp_header.type) {
        case NET_IPV4_ICMP_ECHO_REPLY:

            net_ipv4_handle_icmp_echo_reply(packet, frame, ipv4_header, &icmp_header);
            break;
        case NET_IPV4_ICMP_ECHO_REQUEST:
            net_ipv4_handle_icmp_echo_request(packet, frame, ipv4_header, &icmp_header);
            break;
        default:
            console_log(LOG_DEBUG, "Net ICMP cannot handle type %u", (uint64_t)icmp_header.type);
            break;
    }
}
