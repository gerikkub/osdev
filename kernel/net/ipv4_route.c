
#include <stdint.h>
#include <string.h>

#include "kernel/console.h"
#include "kernel/assert.h"
#include "kernel/lib/vmalloc.h"
#include "kernel/lib/llist.h"

#include "kernel/net/net.h"
#include "kernel/net/arp.h"
#include "kernel/net/ipv4.h"
#include "kernel/net/ethernet.h"

#include "stdlib/bitutils.h"

typedef struct {
    ipv4_t ip_net;
    uint64_t subnet;
    
    net_dev_t* dev;
} net_ipv4_route_entry_t;

static llist_head_t s_ipv4_route_table;

static net_ipv4_route_entry_t* s_default_route = NULL;

static bool net_route_ipv4_in_subnet(ipv4_t* ip, ipv4_t* ip_net, uint64_t subnet) {
    ipv4_t ip_masked = *ip;

    uint64_t subnet_mask = ~((1 << (32 - subnet)) - 1);

    ip_masked.d[0] &= (subnet_mask >> 24) & 0xFF;
    ip_masked.d[1] &= (subnet_mask >> 16) & 0xFF;
    ip_masked.d[2] &= (subnet_mask >> 8) & 0xFF;
    ip_masked.d[3] &= (subnet_mask) & 0xFF;

    return memcmp(&ip_masked, ip_net, sizeof(ipv4_t)) == 0;
}

void net_route_get_nic_for_ipv4(ipv4_t* dest_ip, net_dev_t** net_dev, ipv4_t* via_ip) {

    net_ipv4_route_entry_t* entry;
    FOR_LLIST(s_ipv4_route_table, entry)

        if (net_route_ipv4_in_subnet(dest_ip, &entry->ip_net, entry->subnet)) {
            *net_dev = entry->dev;
            *via_ip = *dest_ip;
            return;
        }

    END_FOR_LLIST()

    if (s_default_route != NULL) {
        *net_dev = s_default_route->dev;
        *via_ip = s_default_route->ip_net;
        return;
    }

    console_log(LOG_WARN, "Net route no to %d.%d.%d.%d",
                dest_ip->d[0], dest_ip->d[1], dest_ip->d[2], dest_ip->d[3]);

    *net_dev = NULL;
}

void net_route_update_entry(ipv4_t* ip_net, uint64_t subnet, net_dev_t* dev) {

    // TODO: Check for duplicates

    net_ipv4_route_entry_t* new_entry = vmalloc(sizeof(net_ipv4_route_entry_t));
    memcpy(&new_entry->ip_net, ip_net, sizeof(ipv4_t));
    new_entry->subnet = subnet;
    new_entry->dev = dev;

    llist_append_ptr(s_ipv4_route_table, new_entry);
}

void net_route_update_default_entry(ipv4_t* ip_net, uint64_t subnet, net_dev_t* dev) {
    if (s_default_route != NULL) {
        vfree(s_default_route);
    }

    s_default_route = vmalloc(sizeof(net_ipv4_route_entry_t));
    memcpy(&s_default_route->ip_net, ip_net, sizeof(ipv4_t));
    s_default_route->subnet = subnet;
    s_default_route->dev = dev;

}

void net_route_init(void) {
    s_ipv4_route_table = llist_create();
}