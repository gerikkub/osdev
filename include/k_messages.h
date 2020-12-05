
#ifndef __K_MESSAGES_H__
#define __K_MESSAGES_H__

typedef struct __attribute__((packed)) {
    uint64_t msg[4];
} msg_placeholder_t;


typedef enum {
    MSG_TYPE_PAYLOAD = 1,
    MSG_TYPE_MEMORY = 2
} msg_type_t;

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t flags;
    uint16_t dst_id;
    uint16_t port_id;
    uint16_t src_id;
} msg_header_t;

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t flags;
    uint16_t dst;
    uint16_t src;
    uint16_t port;
    uint8_t msg[24];
} system_msg_t;

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t flags;
    uint16_t dst;
    uint16_t src;
    uint16_t port;
    uint8_t payload[24];
} system_msg_payload_t;

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t flags;
    uint16_t dst;
    uint16_t src;
    uint16_t port;
    uint64_t ptr;
    uint64_t len;
    uint8_t payload[8];
} system_msg_memory_t;

#endif