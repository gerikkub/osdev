
#include <stdint.h>
#include <string.h>

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
#include "include/k_ioctl_common.h"

#include "stdlib/bitutils.h"

typedef struct {

    task_t* task;

    void* tcp_listener_ctx;

    ipv4_t our_addr;
    uint16_t our_port;

    llist_head_t incoming_connections;
} net_tcp_bind_ctx_t;

typedef struct {
    fd_ops_t socket_ops;
    void* socket_ctx;
    void* tcp_ctx;
} net_tcp_bind_incoming_t;

hashmap_ctx_t* s_tcp_listen_map = NULL;

void* net_tcp_bind_new_connection(void* ctx, void* tcp_ctx, ipv4_t* their_addr, uint16_t their_port) {

    net_tcp_bind_ctx_t* bind_ctx = ctx;

    net_tcp_bind_incoming_t* new_socket = vmalloc(sizeof(net_tcp_bind_incoming_t));
    new_socket->socket_ctx = net_tcp_socket_create_from_conn(bind_ctx->task,
                                                             tcp_ctx,
                                                             &bind_ctx->our_addr,
                                                             bind_ctx->our_port,
                                                             their_addr,
                                                             their_port,
                                                             &new_socket->socket_ops);
    new_socket->tcp_ctx = tcp_ctx;

    llist_append_ptr(bind_ctx->incoming_connections, new_socket);

    return new_socket->socket_ctx;
}

static int64_t net_tcp_bind_get_incoming(net_tcp_bind_ctx_t* bind_ctx) {

    if (llist_empty(bind_ctx->incoming_connections)) {
        return -1;
    }

    int64_t fd_num = find_open_fd(bind_ctx->task);
    if (fd_num < 0) {
        return -1;
    }

    net_tcp_bind_incoming_t* new_socket = llist_at(bind_ctx->incoming_connections, 0);
    llist_delete_ptr(bind_ctx->incoming_connections, new_socket);

    bind_ctx->task->fds[fd_num].ops = new_socket->socket_ops;
    bind_ctx->task->fds[fd_num].ctx = new_socket->socket_ctx;
    bind_ctx->task->fds[fd_num].valid = true;

    net_tcp_conn_activate_connection(new_socket->tcp_ctx);

    vfree(new_socket);

    return fd_num;
}

static int64_t net_tcp_bind_ioctl_fn(void* ctx, const uint64_t ioctl, const uint64_t* args, const uint64_t arg_count) {

    net_tcp_bind_ctx_t* bind_ctx = ctx;

    switch (ioctl) {
        case BIND_IOCTL_GET_INCOMING:
            return net_tcp_bind_get_incoming(bind_ctx);
        default:
            return -1;
    }
}

static int64_t net_tcp_bind_close_fn(void* ctx) {

    net_tcp_bind_ctx_t* bind_ctx = ctx;

    // TODO: Free listener resources
    //net_tcp_conn_close_listener(bind_ctx->tcp_listener_ctx);

    net_tcp_bind_incoming_t* entry;
    FOR_LLIST(bind_ctx->incoming_connections, entry)
        net_tcp_socket_close_fn(entry->socket_ctx);
        vfree(entry);
    END_FOR_LLIST()

    llist_free_all(bind_ctx->incoming_connections);
    llist_free(bind_ctx->incoming_connections);

    return 0;
}

static const fd_ops_t s_net_tcp_bind_ops = {
    .read = NULL,
    .write = NULL,
    .ioctl = net_tcp_bind_ioctl_fn,
    .close = net_tcp_bind_close_fn
};

int64_t net_tcp_bind_port(k_bind_port_t* bind_port_ctx, fd_ops_t* ops, void** ctx_out) {

    uint64_t listen_port = bind_port_ctx->tcp4.listen_port;
    if (hashmap_contains(s_tcp_listen_map, &listen_port)) {
        return -1;
    }

    net_tcp_bind_ctx_t* bind_ctx = vmalloc(sizeof(net_tcp_bind_ctx_t));

    bind_ctx->task = get_active_task();

    memcpy(&bind_ctx->our_addr, &bind_port_ctx->tcp4.bind_ip, sizeof(ipv4_t));
    bind_ctx->our_port = bind_port_ctx->tcp4.listen_port;
    bind_ctx->incoming_connections = llist_create();

    bind_ctx->tcp_listener_ctx = net_tcp_conn_create_listener(&bind_ctx->our_addr,
                                                          bind_ctx->our_port,
                                                          bind_ctx);

    *ops = s_net_tcp_bind_ops;
    *ctx_out = bind_ctx;

    return 0;
}

void net_tcp_bind_init(void) {
    s_tcp_listen_map = uintmap_alloc(64);
}