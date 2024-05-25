
#pragma once

#include <stdint.h>

#include "kernel/fd.h"

typedef struct {
    int64_t spi_fd;
    fd_ctx_t* gpio_int_fd_ctx;
    int64_t gpio_int_num;
} enc28j60_discover_t;