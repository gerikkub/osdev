
#ifndef __NET_ARP_H__
#define __NET_ARP_H__

#include <stdint.h>

#include "kernel/net/net.h"

#define NET_ARP_MIN_PKT_SIZE 8

#define NET_ARP_HDR_PKT_SIZE (8)

enum {
    NET_ARP_HTYPE_ETHERNET = 1
};

enum {
    NET_ARP_OP_REQUEST = 1,
    NET_ARP_OP_REPLY = 2
};

typedef struct {
    uint16_t htype;
    uint16_t ptype;
    uint8_t hlen;
    uint8_t plen;
    uint16_t oper;

    union {
        struct {
            mac_t sha;
            ipv4_t spa;
            mac_t tha;
            ipv4_t tpa;
        } ipv4;

        uint8_t raw_addrs[0];
    };

} net_arp_packet_t;

typedef struct {
    ipv4_t dest_ipv4;
    mac_t dest_mac;
} net_arp_table_entry_t;

void net_arp_init(void);
void net_arp_table_init(void);

void net_arp_send_packet(net_dev_t* net_dev, net_arp_packet_t* arp_packet, mac_t* dest_mac);

bool net_arp_get_mac_for_ipv4(net_dev_t* net_dev, ipv4_t* ipv4, mac_t* dest_mac);
void net_arp_update_table(ipv4_t* ipv4, mac_t* mac);

#endif
