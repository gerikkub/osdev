
#ifndef __LIB_CIRCBUFFER_H__
#define __LIB_CIRCBUFFER_H__

#include <stdint.h>

typedef struct {

    uint64_t read_idx;
    uint64_t write_idx;

    uint8_t* buffer;
    uint64_t len;

} circbuffer_t;

circbuffer_t* circbuffer_create(uint64_t len);
void circbuffer_destroy(circbuffer_t* circbuffer);

uint64_t circbuffer_len(circbuffer_t* circbuffer);
uint64_t circbuffer_space(circbuffer_t* circbuffer);

uint64_t circbuffer_add(circbuffer_t* circbuffer, const uint8_t* buffer, uint64_t len);
uint64_t circbuffer_get(circbuffer_t* circbuffer, uint8_t* buffer, uint64_t len);
uint64_t circbuffer_peek_idx(circbuffer_t* circbuffer, uint8_t* buffer, uint64_t len, uint64_t peek_idx);
uint64_t circbuffer_del(circbuffer_t* circbuffer, uint64_t len);


#endif
