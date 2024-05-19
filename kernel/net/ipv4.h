
#ifndef __NET_IPV4_H__
#define __NET_IPV4_H__

#include <stdint.h>

#include "kernel/net/net.h"


enum {
    NET_IPV4_PROTO_ICMP = 1,
    NET_IPV4_PROTO_IGMP = 2,
    NET_IPV4_PROTO_TCP = 6,
    NET_IPV4_PROTO_UDP = 17,
    NET_IPV4_PROTO_ENCAP = 41,
    NET_IPV4_PROTO_OSPF = 89,
    NET_IPV4_PROTO_SCTP = 132
};

enum {
    NET_IPV4_F_DF = 1,
    NET_IPV4_F_MF = 2
};

#define NET_IPV4_HEADER_LEN 20

typedef struct {
    uint8_t version;
    uint8_t ihl;
    uint8_t dscp;
    uint8_t ecn;
    uint16_t total_len;
    uint16_t id;
    uint16_t flags;
    uint16_t fragment_offset;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    ipv4_t src_ip;
    ipv4_t dst_ip;

    uint8_t* payload;
    uint64_t payload_len;
} net_ipv4_hdr_t;

void net_ipv4_send_packet(ipv4_t* dest_ip, uint16_t protocol, void* payload, uint64_t payload_len);

void net_ipv4_init();

#endif
