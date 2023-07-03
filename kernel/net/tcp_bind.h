
#ifndef __NET_TCP_BIND_H__
#define __NET_TCP_BIND_H__

#include <stdint.h>

#include "kernel/net/ipv4.h"
#include "kernel/net/tcp_socket.h"
#include "kernel/net/tcp_conn.h"

#include "include/k_net_api.h"

void* net_tcp_bind_new_connection(void* ctx, void* tcp_ctx, ipv4_t* their_addr, uint16_t their_port);
int64_t net_tcp_bind_port(k_bind_port_t* bind_port_ctx, fd_ops_t* ops, void** ctx_out, fd_ctx_t* fd_ctx);
void net_tcp_bind_init(void);

#endif
