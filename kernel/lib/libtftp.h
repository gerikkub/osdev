
#ifndef __LIBTFTP_H__
#define __LIBTFTP_H__

#include <stdint.h>

struct tftp_ctx_;

struct tftp_ctx_* tftp_create_client(int64_t udp_socket_fd);
int64_t tftp_recv_file(struct tftp_ctx_* ctx, const char* fname, void** file_data, int64_t* file_len);

#endif