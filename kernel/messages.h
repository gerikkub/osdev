
#ifndef __MESSAGES_H__
#define __MESSAGES_H__

#include <stdint.h>

#include "include/k_messages.h"

#define MSG_QUEUE_MAXSIZE 64
#define MSG_MAX_DSTS 1024

typedef struct {
    msg_placeholder_t buffer[MSG_QUEUE_MAXSIZE];
    uint64_t read_idx, write_idx;
    uint64_t size;
} msg_queue;

void msg_queue_init(msg_queue* queue);

int64_t syscall_getmsgs(uint64_t msg_buffer,
                        uint64_t msg_buffer_size,
                        uint64_t x2_dummy,
                        uint64_t x3_dummy);

int64_t syscall_sendmsg(uint64_t msg_0,
                        uint64_t msg_1,
                        uint64_t msg_2,
                        uint64_t msg_3);

#endif