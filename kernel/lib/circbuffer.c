
#include <stdint.h>

#include "kernel/assert.h"
#include "kernel/lib/vmalloc.h"
#include "kernel/lib/circbuffer.h"

#include "stdlib/string.h"

circbuffer_t* circbuffer_create(uint64_t len) {

    circbuffer_t* circbuff = vmalloc(sizeof(circbuffer_t));

    circbuff->buffer = vmalloc(len);
    circbuff->len = len;

    circbuff->read_idx = 0;
    circbuff->write_idx = 0;

    return circbuff;
}

void circbuffer_destroy(circbuffer_t* circbuffer) {
    vfree(circbuffer->buffer);
    vfree(circbuffer);
}

uint64_t circbuffer_len(circbuffer_t* circbuffer) {

    if (circbuffer->write_idx >= circbuffer->read_idx) {
        return circbuffer->write_idx - circbuffer->read_idx;
    } else {
        return circbuffer->len - (circbuffer->read_idx - circbuffer->write_idx);
    }
}

uint64_t circbuffer_space(circbuffer_t* circbuffer) {
    return circbuffer->len - circbuffer_len(circbuffer) - 1;
}

uint64_t circbuffer_add(circbuffer_t* circbuffer, const uint8_t* buffer, uint64_t len) {

    uint64_t space = circbuffer_space(circbuffer);

    uint64_t write_len = (space < len) ? space : len;

    uint64_t end_space = circbuffer->len - circbuffer->write_idx;
    if (end_space >= write_len) {
        memcpy(&circbuffer->buffer[circbuffer->write_idx],
               buffer,
               write_len);
    } else {
        memcpy(&circbuffer->buffer[circbuffer->write_idx],
               buffer,
               end_space);
        memcpy(circbuffer->buffer,
               &buffer[end_space],
               write_len - end_space);
    }

    circbuffer->write_idx = (circbuffer->write_idx + write_len) % circbuffer->len;

    return write_len;
}

uint64_t circbuffer_get(circbuffer_t* circbuffer, uint8_t* buffer, uint64_t len) {

    uint64_t items = circbuffer_len(circbuffer);

    uint64_t read_len = (items < len) ? items : len;

    uint64_t end_space = circbuffer->len - circbuffer->read_idx;
    if (end_space >= read_len) {
        memcpy(buffer,
               &circbuffer->buffer[circbuffer->read_idx],
               read_len);
    } else {
        memcpy(buffer,
               &circbuffer->buffer[circbuffer->read_idx],
               end_space);
        memcpy(&buffer[end_space],
               circbuffer->buffer,
               read_len - end_space);
    }

    circbuffer->read_idx = (circbuffer->read_idx + read_len) % circbuffer->len;

    return read_len;
}

uint64_t circbuffer_peek_idx(circbuffer_t* circbuffer, uint8_t* buffer, uint64_t len, uint64_t peek_idx) {

    uint64_t items = circbuffer_len(circbuffer);

    uint64_t read_len;
    if (items >= (len + peek_idx)) {
        read_len = len;
    } else {
        if (items <= peek_idx) {
            return 0;
        } else {
            read_len = peek_idx - items;
        }
    }

    uint64_t start_idx = (circbuffer->read_idx + peek_idx) % circbuffer->len;
    uint64_t end_space = circbuffer->len - (start_idx);

    if (end_space >= read_len) {
        memcpy(buffer,
               &circbuffer->buffer[start_idx],
               read_len);
    } else {
        memcpy(buffer,
               &circbuffer->buffer[start_idx],
               end_space);
        memcpy(&buffer[end_space],
               circbuffer->buffer,
               read_len - end_space);
    }

    return read_len;
}

uint64_t circbuffer_del(circbuffer_t* circbuffer, uint64_t len) {

    uint64_t have_len = circbuffer_len(circbuffer);
    uint64_t del_len;

    del_len = len < have_len ? len : have_len;

    circbuffer->read_idx = (circbuffer->read_idx + del_len) % circbuffer->len;

    return del_len;
}