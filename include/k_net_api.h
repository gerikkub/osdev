
#ifndef __K_NET_API_H__
#define __K_NET_API_H__

enum {
    SYSCALL_SOCKET_UDP4 = 1,
    SYSCALL_SOCKET_TCP4 = 2
};

enum {
    SYSCALL_BIND_TCP4 = 2,
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

        struct {
            k_ipv4_t dest_ip;
            uint16_t dest_port;
        } tcp4;
    };
} k_create_socket_t;

typedef struct {
    uint8_t bind_type;

    union {
        struct {
            k_ipv4_t bind_ip;
            uint16_t listen_port;
        } tcp4;
    };
} k_bind_port_t;

typedef struct {
    uint8_t socket_type;

    union {
        struct {
            k_ipv4_t dest_ip;
            uint16_t source_port;
            uint16_t dest_port;
        } udp4;

        struct {
            k_ipv4_t dest_ip;
            uint16_t source_port;
            uint16_t dest_port;
        } tcp4;
    };
} k_socket_info_t;

#endif
