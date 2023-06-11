
#ifndef __NET_IPV4_ICMP_H__
#define __NET_IPV4_ICMP_H__

#include <stdint.h>

#include "kernel/net/net.h"

enum {
    NET_IPV4_ICMP_ECHO_REPLY = 0,
    NET_IPV4_ICMP_DEST_UNREACHABLE = 3,
    NET_IPV4_ICMP_TIME_EXCEEDED = 11,
    NET_IPV4_ICMP_ECHO_REQUEST = 8,
    NET_IPV4_ICMP_TIMESTAMP = 13,
    NET_IPV4_ICMP_TIMESTAMP_REPLAY = 14,
};


typedef struct {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;

    union {
        struct {
            uint16_t id;
            uint16_t seq;
        } echo;
    };

    uint8_t* payload;
    uint64_t payload_len;
} net_ipv4_icmp_hdr_t;

void net_ipv4_handle_icmp(net_packet_t* packet, ethernet_l2_frame_t* frame, net_ipv4_hdr_t* ipv4_header);


#endif
