
#ifndef __NIC_OPS_H__
#define __NIC_OPS_H__

#include <stdint.h>
#include <stdbool.h>

#include "kernel/fd.h"

struct net_dev;
struct net_packet;
struct net_send_buffer;

typedef struct net_send_buffer* (*nic_get_buffer_op)(struct net_dev* ctx, const int64_t size, const uint64_t flags);
typedef void (*nic_send_buffer_op)(struct net_dev* ctx, struct net_send_buffer* buffer);
typedef void (*nic_free_buffer_op)(struct net_dev* ctx, struct net_send_buffer* buffer);
typedef void (*nic_return_packet_op)(struct net_packet* packet);

typedef struct {
    nic_get_buffer_op get_buffer;
    nic_send_buffer_op send_buffer;
    nic_free_buffer_op free_buffer;
    nic_return_packet_op return_packet;
    fd_ioctl_op ioctl;
} nic_ops_t;

#endif
