
#ifndef __SYSTEM_SOCKET_H__
#define __SYSTEM_SOCKET_H__

#include <stdint.h>

#include "include/k_net_api.h"

int64_t system_socket(k_create_socket_t* socket_ptr);
int64_t system_bind(k_bind_port_t* bind_ptr);

#endif
