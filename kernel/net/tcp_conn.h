
#ifndef __NET_TCP_CONN_H__
#define __NET_TCP_CONN_H__

#include <stdint.h>

#include "kernel/lib/circbuffer.h"
#include "kernel/net/net.h"
#include "kernel/net/ipv4.h"
#include "kernel/net/tcp.h"

enum {
    NET_TCP_CONN_SM_LISTEN = 0,
    NET_TCP_CONN_SM_SYN_SENT,
    NET_TCP_CONN_SM_SYN_RECEIVED,
    NET_TCP_CONN_SM_ESTABLISHED,
    NET_TCP_CONN_SM_FIN_WAIT_1,
    NET_TCP_CONN_SM_FIN_WAIT_2,
    NET_TCP_CONN_SM_CLOSE_WAIT,
    NET_TCP_CONN_SM_CLOSING,
    NET_TCP_CONN_SM_LAST_ACK,
    NET_TCP_CONN_SM_TIME_WAIT,
    NET_TCP_CONN_SM_CLOSED
};

// 100 ms
#define NET_TCP_STD_TIMEOUT (100 * 1000 * 1000)
// 60 s
#define NET_TCP_CLOSE_TIMEOUT (60 * 1000 * 1000 * 1000)
#define NET_TCP_WINDOW 4095

typedef struct {
    uint64_t conn_state; // Connection State
    uint64_t mss; // Maximum Segment Size
    uint64_t timeout_expire;
    bool activated;

    // Receiver State
    ipv4_t our_ip;
    uint16_t our_port;
    uint32_t ack_index;
    uint32_t recv_window;

    // Send state
    ipv4_t their_ip;
    uint16_t their_port;
    circbuffer_t* send_buffer;
    uint64_t send_window; // Maximum send windown
    uint32_t seq_index;
    uint32_t sent_index;

    uint64_t force_close_timeout_expire;

    void* socket_ctx;
} net_tcp_conn_ctx_t;

typedef struct {
    ipv4_t listen_addr;
    uint16_t listen_port;

    void* bind_ctx;
} net_tcp_listener_ctx_t;

int64_t net_tcp_conn_recv_data(net_tcp_conn_ctx_t* tcp_ctx, const uint8_t* buffer, uint64_t len);
void net_tcp_conn_init(void);

void* net_tcp_conn_create_listener(ipv4_t* listen_addr, uint16_t listen_port, void* bind_ctx);
void* net_tcp_conn_create_client(ipv4_t* our_addr, ipv4_t* their_addr, uint16_t our_port, uint16_t their_port, void* socket_ctx);
void net_tcp_conn_activate_connection(net_tcp_conn_ctx_t* tcp_ctx);

void net_tcp_conn_close_from_socket(net_tcp_conn_ctx_t* tcp_ctx);
void net_tcp_conn_close_listener(net_tcp_conn_ctx_t* tcp_ctx);

void net_tcp_handle_connection(net_packet_t* packet, net_ipv4_hdr_t* ipv4_header, net_tcp_hdr_t* tcp_header);
void net_tcp_timeout_thread(void* ctx);

#endif
