
#include <stdint.h>

#include "kernel/console.h"
#include "kernel/assert.h"
#include "kernel/lib/vmalloc.h"
#include "kernel/lib/hashmap.h"

#include "kernel/net/net.h"
#include "kernel/net/arp.h"
#include "kernel/net/ethernet.h"

#include "stdlib/bitutils.h"
#include "stdlib/string.h"

static llist_head_t s_arp_table = NULL;

static void net_arp_make_ipv4_request(net_dev_t* net_dev, ipv4_t* ipv4, mac_t* mac) {

    net_arp_packet_t req_packet = {
        .htype = NET_ARP_HTYPE_ETHERNET,
        .ptype = NET_ETHERTYPE_IPV4,
        .hlen = 6,
        .plen = 4,
        .oper = NET_ARP_OP_REQUEST
    };

    memcpy(&req_packet.ipv4.sha, &net_dev->mac, sizeof(mac_t));
    memcpy(&req_packet.ipv4.spa, &net_dev->ipv4, sizeof(ipv4_t));
    memcpy(&req_packet.ipv4.tha, "\xFF\xFF\xFF\xFF\xFF\xFF", sizeof(mac_t));
    memcpy(&req_packet.ipv4.tpa, "\x00\x00\x00\x00", sizeof(ipv4_t));

    console_log(LOG_DEBUG, "Net arp table sending request for %d.%d.%d.%d",
                ipv4->d[0], ipv4->d[1], ipv4->d[2], ipv4->d[3]);

    net_arp_send_packet(net_dev, &req_packet, &req_packet.ipv4.tha);
}

void net_arp_update_table(ipv4_t* ipv4, mac_t* mac) {

    net_arp_table_entry_t* entry;
    net_arp_table_entry_t* found_entry = NULL;
    FOR_LLIST(s_arp_table, entry)
        if (memcmp(&entry->dest_ipv4, ipv4, sizeof(ipv4_t)) == 0) {
            found_entry = entry;
        }
    END_FOR_LLIST()

    if (found_entry != NULL) {
        llist_delete_ptr(s_arp_table, entry);
    }

    net_arp_table_entry_t* new_entry = vmalloc(sizeof(net_arp_table_entry_t));
    memcpy(&new_entry->dest_ipv4, ipv4, sizeof(ipv4_t));
    memcpy(&new_entry->dest_mac, mac, sizeof(mac_t));

    llist_append_ptr(s_arp_table, new_entry);

    console_log(LOG_DEBUG, "Net arp add entry %d.%d.%d.%d at %2x:%2x:%2x:%2x:%2x:%2x:",
                ipv4->d[0], ipv4->d[1], ipv4->d[2], ipv4->d[3],
                mac->d[0], mac->d[1], mac->d[2],
                mac->d[3], mac->d[4], mac->d[5]);
}

bool net_arp_get_mac_for_ipv4(net_dev_t* net_dev, ipv4_t* ipv4, mac_t* dest_mac) {

    net_arp_table_entry_t* entry;
    FOR_LLIST(s_arp_table, entry)
        if (memcmp(&entry->dest_ipv4, ipv4, sizeof(ipv4_t)) == 0) {
            *dest_mac = entry->dest_mac;
            return true;
        }
    END_FOR_LLIST()

    net_arp_make_ipv4_request(net_dev, ipv4, dest_mac);

    return false;
}

void net_arp_table_init(void) {
    s_arp_table = llist_create();
}