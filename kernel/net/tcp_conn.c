
#include <stdint.h>
#include <string.h>

#include "kernel/console.h"
#include "kernel/assert.h"
#include "kernel/gtimer.h"
#include "kernel/task.h"
#include "kernel/lib/vmalloc.h"
#include "kernel/lib/hashmap.h"

#include "kernel/net/net.h"
#include "kernel/net/ipv4.h"
#include "kernel/net/tcp.h"
#include "kernel/net/tcp_conn.h"
#include "kernel/net/tcp_socket.h"
#include "kernel/net/tcp_bind.h"

#include "stdlib/bitutils.h"

typedef struct {
    ipv4_t our_ip;
    uint16_t our_port;
    ipv4_t their_ip;
    uint16_t their_port;
} net_tcp_conn_key_t;

static hashmap_ctx_t* s_tcp_conn_map = NULL;
static hashmap_ctx_t* s_tcp_listener_map = NULL;

static uint64_t net_tcp_conn_random(void) {
    return (gtimer_get_count() / 1000) % 10000;
}

void net_tcp_reset_timeout(net_tcp_conn_ctx_t* tcp_ctx, uint64_t delta) {
    tcp_ctx->timeout_expire = gtimer_get_count() + delta;
}

void net_tcp_send_fin(net_tcp_conn_ctx_t* tcp_ctx) {

    net_tcp_hdr_t resp_header = {
        .source_port = tcp_ctx->our_port,
        .dest_port = tcp_ctx->their_port,
        .seq_num = tcp_ctx->seq_index,
        .ack_num = tcp_ctx->ack_index,
        .doff = 5,
        .f_cwr = 0,
        .f_ece = 0,
        .f_urg = 0,
        .f_ack = 1,
        .f_psh = 0,
        .f_rst = 0,
        .f_syn = 0,
        .f_fin = 1,
        .window_size = tcp_ctx->recv_window,
        .checksum = 0,
        .urgent_pointer = 0,
        .payload = NULL,
        .payload_len = 0
    };

    net_tcp_send_packet(&tcp_ctx->their_ip, &resp_header);
}

void net_tcp_send_ack(net_tcp_conn_ctx_t* tcp_ctx) {

    net_tcp_hdr_t resp_header = {
        .source_port = tcp_ctx->our_port,
        .dest_port = tcp_ctx->their_port,
        .seq_num = tcp_ctx->seq_index,
        .ack_num = tcp_ctx->ack_index,
        .doff = 5,
        .f_cwr = 0,
        .f_ece = 0,
        .f_urg = 0,
        .f_ack = 1,
        .f_psh = 0,
        .f_rst = 0,
        .f_syn = 0,
        .f_fin = 0,
        .window_size = tcp_ctx->recv_window,
        .checksum = 0,
        .urgent_pointer = 0,
        .payload = NULL,
        .payload_len = 0
    };

    net_tcp_send_packet(&tcp_ctx->their_ip, &resp_header);
}

void net_tcp_send_syn(net_tcp_conn_ctx_t* tcp_ctx) {

    net_tcp_hdr_t resp_header = {
        .source_port = tcp_ctx->our_port,
        .dest_port = tcp_ctx->their_port,
        .seq_num = tcp_ctx->seq_index,
        .ack_num = tcp_ctx->ack_index,
        .doff = 5,
        .f_cwr = 0,
        .f_ece = 0,
        .f_urg = 0,
        .f_ack = 0,
        .f_psh = 0,
        .f_rst = 0,
        .f_syn = 1,
        .f_fin = 0,
        .window_size = tcp_ctx->recv_window,
        .checksum = 0,
        .urgent_pointer = 0,
        .payload = NULL,
        .payload_len = 0
    };

    net_tcp_send_packet(&tcp_ctx->their_ip, &resp_header);
}

void net_tcp_send_syn_ack(net_tcp_conn_ctx_t* tcp_ctx) {

    net_tcp_hdr_t resp_header = {
        .source_port = tcp_ctx->our_port,
        .dest_port = tcp_ctx->their_port,
        .seq_num = tcp_ctx->seq_index,
        .ack_num = tcp_ctx->ack_index,
        .doff = 5,
        .f_cwr = 0,
        .f_ece = 0,
        .f_urg = 0,
        .f_ack = 1,
        .f_psh = 0,
        .f_rst = 0,
        .f_syn = 1,
        .f_fin = 0,
        .window_size = tcp_ctx->recv_window,
        .checksum = 0,
        .urgent_pointer = 0,
        .payload = NULL,
        .payload_len = 0
    };

    net_tcp_send_packet(&tcp_ctx->their_ip, &resp_header);
}

void net_tcp_send_std(net_tcp_conn_ctx_t* tcp_ctx, uint8_t* payload, uint64_t payload_len) {

    net_tcp_hdr_t resp_header = {
        .source_port = tcp_ctx->our_port,
        .dest_port = tcp_ctx->their_port,
        .seq_num = tcp_ctx->seq_index,
        .ack_num = tcp_ctx->ack_index,
        .doff = 5,
        .f_cwr = 0,
        .f_ece = 0,
        .f_urg = 0,
        .f_ack = 1,
        .f_psh = 1,
        .f_rst = 0,
        .f_syn = 0,
        .f_fin = 0,
        .window_size = tcp_ctx->recv_window,
        .checksum = 0,
        .urgent_pointer = 0,
        .payload = payload,
        .payload_len = payload_len
    };

    net_tcp_send_packet(&tcp_ctx->their_ip, &resp_header);
}

void* net_tcp_conn_create_listener(ipv4_t* listen_addr, uint16_t listen_port, void* bind_ctx) {
    net_tcp_conn_key_t key = {
        .their_ip.d = {0},
        .their_port = 0,
        .our_ip = *listen_addr,
        .our_port = listen_port
    };

    if (hashmap_contains(s_tcp_conn_map, &key)) {
        return NULL;
    }

    net_tcp_listener_ctx_t* listener_ctx = vmalloc(sizeof(net_tcp_listener_ctx_t));
    net_tcp_conn_key_t* new_key = vmalloc(sizeof(net_tcp_conn_key_t));
    *new_key = key;

    listener_ctx->listen_addr = *listen_addr;
    listener_ctx->listen_port = listen_port;
    listener_ctx->bind_ctx = bind_ctx;

    hashmap_add(s_tcp_listener_map, new_key, listener_ctx);

    console_log(LOG_DEBUG, "Net TCP created listener on %u.%u.%u.%u:%u",
                LOG_IPV4_ADDR(*listen_addr), listen_port);

    return listener_ctx;
}

void* net_tcp_conn_create_client(ipv4_t* our_addr, ipv4_t* their_addr, uint16_t our_port, uint16_t their_port, void* socket_ctx) {

    net_tcp_conn_key_t* new_key = vmalloc(sizeof(net_tcp_conn_key_t));
    net_tcp_conn_ctx_t* new_ctx = vmalloc(sizeof(net_tcp_conn_ctx_t));

    new_key->our_ip = *our_addr;
    new_key->our_port = our_port;
    new_key->their_ip = *their_addr;
    new_key->their_port = their_port;

    new_ctx->conn_state = NET_TCP_CONN_SM_SYN_SENT;
    new_ctx->mss = 1000;

    new_ctx->our_ip = new_key->our_ip;
    new_ctx->our_port = new_key->our_port;
    new_ctx->ack_index = 0;
    new_ctx->recv_window = NET_TCP_WINDOW;

    new_ctx->their_ip = new_key->their_ip;
    new_ctx->their_port = new_key->their_port;
    new_ctx->send_buffer = circbuffer_create(4096);
    new_ctx->send_window = 0;
    new_ctx->seq_index = net_tcp_conn_random();
    new_ctx->sent_index = new_ctx->seq_index;

    new_ctx->force_close_timeout_expire = UINT64_MAX;

    new_ctx->socket_ctx = socket_ctx;

    net_tcp_reset_timeout(new_ctx, NET_TCP_STD_TIMEOUT);

    hashmap_add(s_tcp_conn_map, new_key, new_ctx);

    net_tcp_send_syn(new_ctx);

    return new_ctx;
}


void net_tcp_handle_new_connection(net_ipv4_hdr_t* ipv4_header, net_tcp_hdr_t* tcp_header, net_tcp_listener_ctx_t* ctx) {

    if (!tcp_header->f_syn) {
        return;
    }

    net_tcp_conn_key_t* new_key = vmalloc(sizeof(net_tcp_conn_key_t));
    net_tcp_conn_ctx_t* new_ctx = vmalloc(sizeof(net_tcp_conn_ctx_t));

    new_key->our_ip = ipv4_header->dst_ip;
    new_key->our_port = tcp_header->dest_port;
    new_key->their_ip = ipv4_header->src_ip;
    new_key->their_port = tcp_header->source_port;

    new_ctx->conn_state = NET_TCP_CONN_SM_SYN_RECEIVED;
    new_ctx->mss = 1000;
    new_ctx->activated = false;

    new_ctx->our_ip = new_key->our_ip;
    new_ctx->our_port = new_key->our_port;
    new_ctx->ack_index = tcp_header->seq_num + 1;
    new_ctx->recv_window = NET_TCP_WINDOW;

    new_ctx->their_ip = new_key->their_ip;
    new_ctx->their_port = new_key->their_port;
    new_ctx->send_buffer = circbuffer_create(4096);
    new_ctx->send_window = 0;
    new_ctx->seq_index = net_tcp_conn_random();
    new_ctx->sent_index = new_ctx->seq_index;

    new_ctx->force_close_timeout_expire = UINT64_MAX;

    new_ctx->socket_ctx = net_tcp_bind_new_connection(ctx->bind_ctx, new_ctx, &new_ctx->their_ip, new_ctx->their_port);

    net_tcp_reset_timeout(new_ctx, NET_TCP_STD_TIMEOUT);

    hashmap_add(s_tcp_conn_map, new_key, new_ctx);
}

void net_tcp_conn_activate_connection(net_tcp_conn_ctx_t* tcp_ctx) {
    ASSERT(tcp_ctx->conn_state == NET_TCP_CONN_SM_SYN_RECEIVED);

    tcp_ctx->activated = true;

    net_tcp_send_syn_ack(tcp_ctx);
}

uint32_t net_tcp_32wrap_diff(uint32_t num1, uint32_t num2) {
    if (num1 >= num2) {
        return num1 - num2;
    } else {
        return num2 + (UINT32_MAX - (uint64_t)num1);
    }
}

void net_tcp_conn_send_segment(net_tcp_conn_ctx_t* tcp_ctx) {

    if (tcp_ctx->conn_state != NET_TCP_CONN_SM_ESTABLISHED) {
        return;
    }

    ASSERT(tcp_ctx->send_buffer != NULL);
    uint64_t send_buffer_len = circbuffer_len(tcp_ctx->send_buffer);
    if (send_buffer_len == 0) {
        return;
    }

    uint32_t window_right = tcp_ctx->seq_index + tcp_ctx->send_window;

    uint32_t window_room = net_tcp_32wrap_diff(window_right, tcp_ctx->sent_index);

    if (window_room > 0) {
        uint64_t max_send_size = send_buffer_len > window_room ? window_room : send_buffer_len;
        uint64_t send_offset = net_tcp_32wrap_diff(tcp_ctx->sent_index, tcp_ctx->seq_index);

        ASSERT(tcp_ctx->send_buffer != NULL);
        uint8_t* buffer = vmalloc(max_send_size);

        ASSERT(tcp_ctx->send_buffer != NULL);
        uint64_t send_size = circbuffer_peek_idx(tcp_ctx->send_buffer, buffer, max_send_size, send_offset);

        if (send_size > 0) {
            tcp_ctx->sent_index += send_size;
            net_tcp_send_std(tcp_ctx, buffer, send_size);
        }

        vfree(buffer);

        net_tcp_reset_timeout(tcp_ctx, 1*1000*1000);
    }
}

void net_tcp_conn_recv_segment(net_tcp_hdr_t* tcp_header, net_tcp_conn_ctx_t* tcp_ctx) {

    int64_t recv_len = net_tcp_socket_recv(tcp_ctx->socket_ctx,
                                           tcp_header->payload,
                                           tcp_header->payload_len);

    // Don't ack packets if they can't be queued
    if (recv_len > 0) {
        tcp_ctx->ack_index += recv_len;
        net_tcp_send_ack(tcp_ctx);
    }

}

void net_tcp_handle_conn_syn_received(net_tcp_hdr_t* tcp_header, net_tcp_conn_ctx_t* tcp_ctx) {

    if (tcp_ctx->activated) {
        if (tcp_header->f_ack &&
            tcp_header->ack_num == (tcp_ctx->seq_index + 1)) {

            tcp_ctx->seq_index += 1;
            tcp_ctx->sent_index = tcp_ctx->seq_index;
            tcp_ctx->conn_state = NET_TCP_CONN_SM_ESTABLISHED;
            tcp_ctx->send_window = tcp_header->window_size;

            net_tcp_conn_send_segment(tcp_ctx);
        } else if (tcp_header->f_syn) {
            // Got another SYN packet. Try to ack again
            tcp_ctx->ack_index = tcp_header->seq_num + 1;

            net_tcp_send_syn_ack(tcp_ctx);
        }
    }
}

void net_tcp_handle_conn_syn_sent(net_tcp_hdr_t* tcp_header, net_tcp_conn_ctx_t* tcp_ctx) {
    if (tcp_header->f_ack &&
        tcp_header->ack_num == (tcp_ctx->seq_index + 1) &&
        tcp_header->f_syn) {

        tcp_ctx->seq_index += 1;
        tcp_ctx->sent_index = tcp_ctx->seq_index;
        tcp_ctx->conn_state = NET_TCP_CONN_SM_ESTABLISHED;
        tcp_ctx->ack_index = tcp_header->seq_num + 1;
        tcp_ctx->send_window = tcp_header->window_size;

        net_tcp_send_ack(tcp_ctx);

        net_tcp_conn_send_segment(tcp_ctx);
    } else if (tcp_header->f_syn) {
        // Simultaneous Open
        tcp_ctx->conn_state = NET_TCP_CONN_SM_SYN_RECEIVED;
        tcp_ctx->ack_index = tcp_header->seq_num + 1;

        net_tcp_send_ack(tcp_ctx);
    }
}

void net_tcp_handle_conn_fin_wait_1(net_tcp_hdr_t* tcp_header, net_tcp_conn_ctx_t* tcp_ctx) {

    if (tcp_header->f_fin && tcp_header->f_ack) {
        tcp_ctx->conn_state = NET_TCP_CONN_SM_CLOSING;
        tcp_ctx->ack_index = tcp_header->seq_num;
        tcp_ctx->seq_index = tcp_header->ack_num;

        net_tcp_send_ack(tcp_ctx);
    } else if (tcp_header->f_ack &&
               tcp_header->seq_num == tcp_ctx->ack_index &&
               tcp_header->ack_num == tcp_ctx->seq_index) {
        tcp_ctx->conn_state = NET_TCP_CONN_SM_FIN_WAIT_2;
    }
}

void net_tcp_handle_conn_fin_wait_2(net_tcp_hdr_t* tcp_header, net_tcp_conn_ctx_t* tcp_ctx) {

    if (tcp_header->f_fin &&
        tcp_header->f_ack) {
        tcp_ctx->conn_state = NET_TCP_CONN_SM_TIME_WAIT;
        tcp_ctx->ack_index = tcp_header->seq_num;
        tcp_ctx->seq_index = tcp_header->ack_num;

        net_tcp_send_ack(tcp_ctx);
    }
}

void net_tcp_handle_conn_closing(net_tcp_hdr_t* tcp_header, net_tcp_conn_ctx_t* tcp_ctx) {
    if (tcp_header->f_ack) {
        tcp_ctx->conn_state = NET_TCP_CONN_SM_TIME_WAIT;
    }
}

void net_tcp_handle_conn_close_wait(net_tcp_hdr_t* tcp_header, net_tcp_conn_ctx_t* tcp_ctx) {
    if (tcp_header->f_fin) {
        tcp_ctx->ack_index = tcp_header->seq_num + 1;

        net_tcp_send_ack(tcp_ctx);
    }

}

void net_tcp_handle_conn_last_ack(net_tcp_hdr_t* tcp_header, net_tcp_conn_ctx_t* tcp_ctx) {
    if (tcp_header->f_ack &&
        tcp_header->ack_num == tcp_ctx->seq_index + 1) {

        tcp_ctx->conn_state = NET_TCP_CONN_SM_CLOSED;
    } else {
        tcp_ctx->seq_index += 1;
        tcp_ctx->ack_index = tcp_header->seq_num + 1;

        net_tcp_send_fin(tcp_ctx);
    }
}

void net_tcp_handle_conn_time_wait(net_tcp_hdr_t* tcp_header, net_tcp_conn_ctx_t* tcp_ctx) {
    // Ignore any messages
}

void net_tcp_handle_conn_established(net_tcp_hdr_t* tcp_header, net_tcp_conn_ctx_t* tcp_ctx) {

    tcp_ctx->send_window = tcp_header->window_size;

    if (tcp_header->f_ack) {

        uint64_t bytes_acked = net_tcp_32wrap_diff(tcp_header->ack_num, tcp_ctx->seq_index);

        tcp_ctx->seq_index = tcp_header->ack_num;
        circbuffer_del(tcp_ctx->send_buffer, bytes_acked);
    }

    if (tcp_header->payload_len > 0) {
        net_tcp_conn_recv_segment(tcp_header, tcp_ctx);
    }

    if (tcp_header->f_fin && tcp_header->f_ack) { 
        tcp_ctx->conn_state = NET_TCP_CONN_SM_CLOSE_WAIT;
        tcp_ctx->ack_index = tcp_header->seq_num + 1;
        
        net_tcp_socket_close(tcp_ctx->socket_ctx, false);
        tcp_ctx->socket_ctx = NULL;
        return;
    }

    ASSERT(tcp_ctx->conn_state == NET_TCP_CONN_SM_ESTABLISHED);
    ASSERT(tcp_ctx->send_buffer != NULL);
    net_tcp_conn_send_segment(tcp_ctx);
}

void net_tcp_conn_close_from_socket(net_tcp_conn_ctx_t* tcp_ctx) {
    tcp_ctx->conn_state = NET_TCP_CONN_SM_LAST_ACK;
    net_tcp_send_fin(tcp_ctx);

    tcp_ctx->socket_ctx = NULL;
}

void net_tcp_conn_cleanup(net_tcp_conn_key_t* conn_key, net_tcp_conn_ctx_t* conn_ctx) {

    //console_log(LOG_DEBUG, "Net TCP Closing Connection (%u.%u.%u.%u:%u %u.%u.%u.%u:%u)",
                //LOG_IPV4_ADDR(conn_key->their_ip), conn_key->their_port,
                //LOG_IPV4_ADDR(conn_key->our_ip), conn_key->our_port);

    hashmap_del(s_tcp_conn_map, conn_key);

    if (conn_ctx->socket_ctx != NULL) {
        net_tcp_socket_close(conn_ctx->socket_ctx, true);
        conn_ctx->socket_ctx = NULL;
    }

    if (conn_ctx->send_buffer != NULL) {
        circbuffer_destroy(conn_ctx->send_buffer);
        conn_ctx->send_buffer = NULL;
    }

    vfree(conn_ctx);
}

static char* s_tcp_conn_sm_str[] = {
    "LISTEN",
    "SYN_SENT",
    "SYN_RECEIVED",
    "ESTABLISHED",
    "FIN_WAIT_1",
    "FIN_WAIT_2",
    "CLOSE_WAIT",
    "CLOSING",
    "LAST_ACK",
    "TIME_WAIT",
    "CLOSED"
};


void net_tcp_handle_connection(net_packet_t* packet, net_ipv4_hdr_t* ipv4_header, net_tcp_hdr_t* tcp_header) {

    (void)s_tcp_conn_sm_str;

    //console_log(LOG_DEBUG, "Net TCP received packet");
    //net_tcp_print_packet(&ipv4_header->src_ip, &ipv4_header->dst_ip, tcp_header);

    net_tcp_conn_key_t conn_key = {
        .our_ip = ipv4_header->dst_ip,
        .our_port = tcp_header->dest_port,
        .their_ip = ipv4_header->src_ip,
        .their_port = tcp_header->source_port
    };

    net_tcp_conn_ctx_t* conn_ctx;
    conn_ctx = hashmap_get(s_tcp_conn_map, &conn_key);

    if (conn_ctx == NULL) {

        net_tcp_conn_key_t listen_conn_key = {
            .our_ip = conn_key.our_ip,
            .our_port = conn_key.our_port,
            .their_ip.d = {0},
            .their_port = 0
        };

        net_tcp_listener_ctx_t* listener_ctx;
        listener_ctx = hashmap_get(s_tcp_listener_map, &listen_conn_key);
        if (listener_ctx == NULL) {
            // Handle non-existant connection here
            console_log(LOG_WARN, "Net TCP packet for non-existant listener (%u.%u.%u.%u:%u)",
                        LOG_IPV4_ADDR(listen_conn_key.our_ip), listen_conn_key.our_port);
            return;
        }

        net_tcp_handle_new_connection(ipv4_header, tcp_header, listener_ctx);

    } else {

        //console_log(LOG_DEBUG, "Net TCP state at reception %s",
                    //s_tcp_conn_sm_str[conn_ctx->conn_state]);

        if (tcp_header->f_rst) {
            console_log(LOG_DEBUG, "Net TCP saw reset");
            net_tcp_conn_cleanup(&conn_key, conn_ctx);
        } else {
            net_tcp_reset_timeout(conn_ctx, NET_TCP_STD_TIMEOUT);
            switch (conn_ctx->conn_state) {
                case NET_TCP_CONN_SM_SYN_SENT:
                    net_tcp_handle_conn_syn_sent(tcp_header, conn_ctx);
                    break;
                case NET_TCP_CONN_SM_SYN_RECEIVED:
                    net_tcp_handle_conn_syn_received(tcp_header, conn_ctx);
                    break;
                case NET_TCP_CONN_SM_ESTABLISHED:
                    net_tcp_handle_conn_established(tcp_header, conn_ctx);
                    break;
                case NET_TCP_CONN_SM_FIN_WAIT_1:
                    net_tcp_handle_conn_fin_wait_1(tcp_header, conn_ctx);
                    break;
                case NET_TCP_CONN_SM_FIN_WAIT_2:
                    net_tcp_handle_conn_fin_wait_2(tcp_header, conn_ctx);
                    break;
                case NET_TCP_CONN_SM_CLOSE_WAIT:
                    net_tcp_handle_conn_close_wait(tcp_header, conn_ctx);
                    break;
                case NET_TCP_CONN_SM_CLOSING:
                    net_tcp_handle_conn_closing(tcp_header, conn_ctx);
                    break;
                case NET_TCP_CONN_SM_LAST_ACK:
                    net_tcp_handle_conn_last_ack(tcp_header, conn_ctx);
                    break;
                case NET_TCP_CONN_SM_TIME_WAIT:
                    net_tcp_handle_conn_time_wait(tcp_header, conn_ctx);
                    break;

                case NET_TCP_CONN_SM_LISTEN:
                case NET_TCP_CONN_SM_CLOSED:
                default:
                    ASSERT(0);
                    break;
            }

            if (conn_ctx->conn_state == NET_TCP_CONN_SM_CLOSED) {
                net_tcp_conn_cleanup(&conn_key, conn_ctx);
            }
        }


    }

    //console_log(LOG_DEBUG, "Net TCP state after processing %s",
                //s_tcp_conn_sm_str[conn_ctx->conn_state]);


}

int64_t net_tcp_conn_recv_data(net_tcp_conn_ctx_t* tcp_ctx, const uint8_t* buffer, uint64_t len) {
    int64_t bytes_added = circbuffer_add(tcp_ctx->send_buffer, buffer, len);

    net_tcp_conn_send_segment(tcp_ctx);

    return bytes_added;
}

void net_tcp_timeout_conn_syn_sent(net_tcp_conn_ctx_t* tcp_ctx) {
    net_tcp_send_syn(tcp_ctx);
}

void net_tcp_timeout_conn_syn_received(net_tcp_conn_ctx_t* tcp_ctx) {
    if (tcp_ctx->activated) {
        net_tcp_send_syn_ack(tcp_ctx);
    }
}

void net_tcp_timeout_conn_established(net_tcp_conn_ctx_t* tcp_ctx) {
    net_tcp_conn_send_segment(tcp_ctx);
}

void net_tcp_timeout_conn_fin_wait_1(net_tcp_conn_ctx_t* tcp_ctx) {
    net_tcp_send_fin(tcp_ctx);
}

void net_tcp_timeout_conn_fin_wait_2(net_tcp_conn_ctx_t* tcp_ctx) {
    return;
}

void net_tcp_timeout_conn_close_wait(net_tcp_conn_ctx_t* tcp_ctx) {
    return;
}

void net_tcp_timeout_conn_closing(net_tcp_conn_ctx_t* tcp_ctx) {
    net_tcp_send_ack(tcp_ctx);
}

void net_tcp_timeout_conn_last_ack(net_tcp_conn_ctx_t* tcp_ctx) {
    net_tcp_send_fin(tcp_ctx);
}

void net_tcp_timeout_conn_time_wait(net_tcp_conn_ctx_t* tcp_ctx) {

    net_tcp_conn_key_t key = {
        .our_ip = tcp_ctx->our_ip,
        .our_port = tcp_ctx->our_port,
        .their_ip = tcp_ctx->their_ip,
        .their_port = tcp_ctx->their_port
    };

    net_tcp_conn_cleanup(&key, tcp_ctx);
}

void net_tcp_handle_timeout(net_tcp_conn_ctx_t* tcp_ctx) {

    net_tcp_reset_timeout(tcp_ctx, NET_TCP_STD_TIMEOUT);

    switch (tcp_ctx->conn_state) {
            case NET_TCP_CONN_SM_SYN_SENT:
                net_tcp_timeout_conn_syn_sent(tcp_ctx);
                break;
            case NET_TCP_CONN_SM_SYN_RECEIVED:
                net_tcp_timeout_conn_syn_received(tcp_ctx);
                break;
            case NET_TCP_CONN_SM_ESTABLISHED:
                net_tcp_timeout_conn_established(tcp_ctx);
                break;
            case NET_TCP_CONN_SM_FIN_WAIT_1:
                net_tcp_timeout_conn_fin_wait_1(tcp_ctx);
                break;
            case NET_TCP_CONN_SM_FIN_WAIT_2:
                net_tcp_timeout_conn_fin_wait_2(tcp_ctx);
                break;
            case NET_TCP_CONN_SM_CLOSE_WAIT:
                net_tcp_timeout_conn_close_wait(tcp_ctx);
                break;
            case NET_TCP_CONN_SM_CLOSING:
                net_tcp_timeout_conn_closing(tcp_ctx);
                break;
            case NET_TCP_CONN_SM_LAST_ACK:
                net_tcp_timeout_conn_last_ack(tcp_ctx);
                break;
            case NET_TCP_CONN_SM_TIME_WAIT:
                net_tcp_timeout_conn_time_wait(tcp_ctx);
                break;

            case NET_TCP_CONN_SM_LISTEN:
                break;

            case NET_TCP_CONN_SM_CLOSED:
                ASSERT(0);
                break;
    }
}

void net_tcp_handle_force_timeout(net_tcp_conn_ctx_t* tcp_ctx) {

    switch (tcp_ctx->conn_state) {
        case NET_TCP_CONN_SM_FIN_WAIT_1:
        case NET_TCP_CONN_SM_FIN_WAIT_2:
        case NET_TCP_CONN_SM_CLOSE_WAIT:
        case NET_TCP_CONN_SM_CLOSING:
        case NET_TCP_CONN_SM_LAST_ACK:
        case NET_TCP_CONN_SM_TIME_WAIT:
            break;
        default:
            return;
    }

    net_tcp_conn_key_t key = {
        .our_ip = tcp_ctx->our_ip,
        .our_port = tcp_ctx->our_port,
        .their_ip = tcp_ctx->their_ip,
        .their_port = tcp_ctx->their_port
    };

    console_log(LOG_INFO, "Net TCP force closing connection (%u.%u.%u.%u:%u %u.%u.%u.%u:%u)",
                        LOG_IPV4_ADDR(key.our_ip), key.our_port,
                        LOG_IPV4_ADDR(key.their_ip), key.their_port);

    net_tcp_conn_cleanup(&key, tcp_ctx);
}

void net_tcp_timeout_check(void* ctx, void* check_ctx, void* key, void* dataptr) {

    (void)key;

    uint64_t* curr_time = check_ctx;
    net_tcp_conn_ctx_t* tcp_ctx = dataptr;

    if (*curr_time > tcp_ctx->timeout_expire) {
        net_tcp_handle_timeout(tcp_ctx);
    }

    if (*curr_time > tcp_ctx->force_close_timeout_expire) {
        net_tcp_handle_force_timeout(tcp_ctx);
    }
}

void net_tcp_timeout_thread(void* ctx) {

    while (true) {
        task_wait_timer_in(10 * 1000);
        uint64_t curr_time = gtimer_get_count();
        hashmap_forall(s_tcp_conn_map, net_tcp_timeout_check, &curr_time);
    }
}

uint64_t net_tcp_conn_map_hash_op(void* key) {
    net_tcp_conn_key_t* k = key;
    return k->our_port +
           k->their_port +
           (k->our_ip.d[0] << 24 |
            k->our_ip.d[1] << 16 |
            k->our_ip.d[2] << 8 |
            k->our_ip.d[3]) +
           (k->their_ip.d[0] << 24 |
            k->their_ip.d[1] << 16 |
            k->their_ip.d[2] << 8 |
            k->their_ip.d[3]);
}

bool net_tcp_conn_map_cmp_op(void* key1, void* key2) {
    net_tcp_conn_key_t* k1 = key1;
    net_tcp_conn_key_t* k2 = key2;

    return k1->our_port == k2->our_port &&
           k1->their_port == k2->their_port &&
           k1->our_ip.d[0] == k2->our_ip.d[0] &&
           k1->our_ip.d[1] == k2->our_ip.d[1] &&
           k1->our_ip.d[2] == k2->our_ip.d[2] &&
           k1->our_ip.d[3] == k2->our_ip.d[3] &&
           k1->their_ip.d[0] == k2->their_ip.d[0] &&
           k1->their_ip.d[1] == k2->their_ip.d[1] &&
           k1->their_ip.d[2] == k2->their_ip.d[2] &&
           k1->their_ip.d[3] == k2->their_ip.d[3];
}

void net_tcp_conn_map_free_op(void* ctx, void* key, void* dataptr) {
    vfree(key);
}

void net_tcp_conn_init(void) {
    s_tcp_conn_map = hashmap_alloc(net_tcp_conn_map_hash_op,
                                   net_tcp_conn_map_cmp_op,
                                   net_tcp_conn_map_free_op,
                                   256,
                                   NULL);

    s_tcp_listener_map = hashmap_alloc(net_tcp_conn_map_hash_op,
                                       net_tcp_conn_map_cmp_op,
                                       net_tcp_conn_map_free_op,
                                       256,
                                       NULL);
                                
    create_kernel_task(256*1024, net_tcp_timeout_thread, NULL, "tcpconn");
}

void net_tcp_conn_start_timeout_thread(void) {
}