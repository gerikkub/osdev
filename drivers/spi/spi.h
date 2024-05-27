
#pragma once

#include <stdint.h>

#include "include/k_spi.h"

struct spi_txn;
struct spi_device_ctx_;

typedef struct spi_txn {
    uint64_t len;

    uint64_t write_pos;
    uint8_t* write_mem;

    uint64_t read_pos;
    uint8_t* read_mem;

    bool complete;

    struct spi_device_ctx_* device_ctx;
} spi_txn_t;

typedef int64_t (*spi_execute_txn_fn)(void*, k_spi_device_t*, spi_txn_t*);

typedef struct {
    spi_execute_txn_fn execute_fn;
} spi_ops_t;

int64_t spi_create_device(void* driver_ctx, spi_ops_t* ops, k_spi_device_t* new_device);

void spi_txn_complete(spi_txn_t* txn);