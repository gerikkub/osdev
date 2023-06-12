
#ifndef __K_NET_API_H__
#define __K_NET_API_H__

enum {
    SYSCALL_SOCKET_UDP4 = 1
};

enum {
    K_SOCKET_READ_FLAGS_NONBLOCKING = 1
};

typedef struct {
    uint8_t d[4];
} k_ipv4_t;

typedef struct {
    uint8_t socket_type;

    union {
        struct {
            k_ipv4_t dest_ip;
            uint16_t source_port;
            uint16_t dest_port;
        } udp4;
    };
} k_create_socket_t;

#endif
