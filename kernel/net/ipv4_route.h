
#ifndef __NET_IPV4_ROUTE_H__
#define __NET_IPV4_ROUTE_H__

#include <stdint.h>

#include "kernel/net/net.h"

void net_route_get_nic_for_ipv4(ipv4_t* dest_ip, net_dev_t** net_dev, ipv4_t* via_ip);
void net_route_update_entry(ipv4_t* ip_net, uint64_t subnet, net_dev_t* dev);
void net_route_update_default_entry(ipv4_t* ip_net, uint64_t subnet, net_dev_t* dev);
void net_route_init(void);

#endif
