
#include <stdint.h>

#include "kernel/console.h"
#include "kernel/assert.h"
#include "kernel/task.h"
#include "kernel/lib/vmalloc.h"
#include "kernel/lib/intmap.h"
#include "kernel/lib/hashmap.h"
#include "kernel/lib/llist.h"
#include "kernel/lib/circbuffer.h"

#include "kernel/net/net.h"
#include "kernel/net/arp.h"
#include "kernel/net/ipv4.h"
#include "kernel/net/ipv4_icmp.h"
#include "kernel/net/ipv4_route.h"
#include "kernel/net/tcp.h"
#include "kernel/net/tcp_conn.h"
#include "kernel/net/tcp_socket.h"
#include "kernel/net/ethernet.h"

#include "include/k_net_api.h"

#include "stdlib/bitutils.h"
#include "stdlib/string.h"

typedef struct {

    task_t* task;

    bool should_close;
    void* tcp_conn_ctx;
    circbuffer_t* recv_buffer;

    ipv4_t our_ip;
    uint16_t our_port;
    ipv4_t their_ip;
    uint16_t their_port;
} net_tcp_socket_ctx_t;

hashmap_ctx_t* s_tcp_socket_map = NULL;


int64_t net_tcp_socket_recv(void* ctx, const uint8_t* payload, uint64_t payload_len) {

    net_tcp_socket_ctx_t* socket_ctx = ctx;

    int64_t space_avail = circbuffer_space(socket_ctx->recv_buffer);
    if (space_avail < payload_len) {
        return -1;
    }

    circbuffer_add(socket_ctx->recv_buffer, payload, payload_len);

    return payload_len;
}

void net_tcp_socket_close(void* ctx, bool dontreply) {
    net_tcp_socket_ctx_t* socket_ctx = ctx;

    socket_ctx->should_close = true;
    if (dontreply) {
        socket_ctx->tcp_conn_ctx = NULL;
    }
}

static int64_t net_tcp_socket_read_fn(void* ctx, uint8_t* buffer, const int64_t size, const uint64_t flags) {

    net_tcp_socket_ctx_t* socket_ctx = ctx;

    int64_t bytes_avail = circbuffer_len(socket_ctx->recv_buffer);
    int64_t bytes_read = (bytes_avail < size) ? bytes_avail : size;

    if (bytes_read > 0) {
        circbuffer_get(socket_ctx->recv_buffer, buffer, bytes_read);
        return bytes_read;
    } else {

        if (socket_ctx->should_close) {
            return -1;
        }

        if (flags & K_SOCKET_READ_FLAGS_NONBLOCKING) {
            return 0;
        } else {
            ASSERT(0);
            return 0;
        }
    }
}

static int64_t net_tcp_socket_write_fn(void* ctx, const uint8_t* buffer, const int64_t size, const uint64_t flags) {
    net_tcp_socket_ctx_t* socket_ctx = ctx;

    if (socket_ctx->should_close) {
        return -1;
    }

    int64_t wrote = net_tcp_conn_recv_data(socket_ctx->tcp_conn_ctx, buffer, size);

    return wrote;
}

static int64_t net_tcp_socket_ioctl_fn(void* ctx, const uint64_t ioctl, const uint64_t* args, const uint64_t arg_count) {
    return -1;
}

int64_t net_tcp_socket_close_fn(void* ctx) {

    net_tcp_socket_ctx_t* socket_ctx = ctx;

    if (socket_ctx->tcp_conn_ctx != NULL) {
        net_tcp_conn_close_from_socket(socket_ctx->tcp_conn_ctx);
    }


    circbuffer_destroy(socket_ctx->recv_buffer);

    vfree(socket_ctx);

    return 0;
}

static const fd_ops_t s_net_tcp_socket_ops = {
    .read = net_tcp_socket_read_fn,
    .write = net_tcp_socket_write_fn,
    .ioctl = net_tcp_socket_ioctl_fn,
    .close = net_tcp_socket_close_fn,
};

void* net_tcp_socket_create_from_conn(task_t* task, void* tcp_ctx, ipv4_t* our_ip, uint16_t our_port, ipv4_t* their_ip, uint16_t their_port, fd_ops_t* ops) {

    // TODO: How should s_tcp_socket_map be dealt with?

    net_tcp_socket_ctx_t* socket_ctx = vmalloc(sizeof(net_tcp_socket_ctx_t));

    socket_ctx->task = task;
    socket_ctx->should_close = false;
    socket_ctx->tcp_conn_ctx = tcp_ctx;
    socket_ctx->recv_buffer = circbuffer_create(4096);
    socket_ctx->our_ip = *our_ip;
    socket_ctx->our_port = our_port;
    socket_ctx->their_ip = *their_ip;
    socket_ctx->their_port = their_port;

    *ops = s_net_tcp_socket_ops;

    return socket_ctx;
}

int64_t net_tcp_create_socket(k_create_socket_t* create_socket_ctx, fd_ops_t* ops, void** ctx_out) {
    
    if (create_socket_ctx->tcp4.dest_port == 0) {
        console_log(LOG_WARN, "Net TCP cannot create socket. Invalid dest port");
        return -1;
    }

    uint64_t listen_port;
    for (listen_port = TCP_EPHIMERAL_START; listen_port < TCP_EPHIMERAL_END; listen_port++) {
        if (!hashmap_contains(s_tcp_socket_map, &listen_port)) {
            break;
        }
    }

    if (listen_port == TCP_EPHIMERAL_END) {
        return -1;
    }

    if (hashmap_contains(s_tcp_socket_map, &listen_port)) {
        console_log(LOG_WARN, "Net TCP cannot create socket. Invalid listen port");
        return -1;
    }

    net_tcp_socket_ctx_t* socket_ctx = vmalloc(sizeof(net_tcp_socket_ctx_t));
    socket_ctx->task = get_active_task();

    memcpy(&socket_ctx->their_ip, &create_socket_ctx->tcp4.dest_ip, sizeof(ipv4_t));
    socket_ctx->their_port = create_socket_ctx->tcp4.dest_port;
    socket_ctx->our_port = listen_port;

    net_dev_t* net_dev = NULL;
    ipv4_t via_ip;
    net_route_get_nic_for_ipv4(&socket_ctx->their_ip, &net_dev, &via_ip);
    if (net_dev == NULL) {
        console_log(LOG_WARN, "Net TCP cannot create socket. No network device for ip");
        vfree(socket_ctx);
        return -1;
    }

    socket_ctx->our_ip = net_dev->ipv4;
    socket_ctx->recv_buffer = circbuffer_create(4096);

    socket_ctx->should_close = false;
    socket_ctx->tcp_conn_ctx = net_tcp_conn_create_client(&socket_ctx->our_ip,
                                                          &socket_ctx->their_ip,
                                                          listen_port,
                                                          create_socket_ctx->tcp4.dest_port,
                                                          socket_ctx);

    uint64_t* hashmap_key = vmalloc(sizeof(uint64_t));
    *hashmap_key = listen_port;
    hashmap_add(s_tcp_socket_map, hashmap_key, socket_ctx);

    *ops = s_net_tcp_socket_ops;
    *ctx_out = socket_ctx;

    return 0;
}

void net_tcp_socket_init(void) {
    s_tcp_socket_map = uintmap_alloc(64);
}