
#ifndef __NET_NET_API_H__
#define __NET_NET_API_H__

#include <stdint.h>

int64_t syscall_socket(uint64_t socket_struct_ptr, uint64_t x1, uint64_t x2, uint64_t x3);
int64_t syscall_bind(uint64_t bind_struct_ptr, uint64_t x1, uint64_t x2, uint64_t x3);

#endif
