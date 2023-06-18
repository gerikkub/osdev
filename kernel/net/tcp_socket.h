
#ifndef __NET_TCP_SOCKET_H__
#define __NET_TCP_SOCKET_H__

#include <stdint.h>

#include "kernel/net/ipv4.h"

#include "include/k_net_api.h"

#define TCP_EPHIMERAL_START 32768
#define TCP_EPHIMERAL_END 65536

int64_t net_tcp_socket_recv(void* ctx, const uint8_t* payload, uint64_t payload_len);

int64_t net_tcp_create_socket(k_create_socket_t* create_socket_ctx, fd_ops_t* ops, void** ctx_out);
void* net_tcp_socket_create_from_conn(void* ctx, ipv4_t* their_ip, uint16_t their_port);
void net_tcp_socket_close(void* ctx);

void net_tcp_socket_init(void);

#endif
