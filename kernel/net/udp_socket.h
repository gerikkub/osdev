
#ifndef __NET_UDP_SOCKET_H__
#define __NET_UDP_SOCKET_H__

#include <stdint.h>

#include "kernel/net/net.h"
#include "kernel/net/ipv4.h"
#include "kernel/net/udp.h"
#include "kernel/fd.h"

#include "include/k_net_api.h"

#define UDP_EPHIMERAL_START 32768

int64_t net_udp_create_socket(k_create_socket_t* create_socket_ctx, fd_ops_t* ops, void** ctx);
void net_udp_socket_recv_packet(net_packet_t* packet, net_ipv4_hdr_t* ipv4_header, net_udp_hdr_t* udp_msg);

#endif
