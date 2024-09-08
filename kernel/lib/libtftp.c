
#include <stdint.h>
#include <string.h>

#include "kernel/console.h"
#include "kernel/assert.h"
#include "kernel/task.h"
#include "kernel/fd.h"
#include "kernel/select.h"
#include "kernel/lib/vmalloc.h"

#include "kernel/net/net.h"
#include "kernel/net/udp.h"
#include "kernel/net/udp_socket.h"

#include "include/k_ioctl_common.h"
#include "include/k_select.h"

#include "stdlib/bitutils.h"

#define TFTP_TIMEOUT_US (50 * 1000)

typedef struct tftp_ctx_ {
    bool conn_created;
    int64_t udp_socket_fd;
    fd_ctx_t* udp_socket;
} tftp_ctx_t;

struct tftp_ctx_* tftp_create_client(int64_t udp_socket_fd) {

    tftp_ctx_t* tftp_ctx = vmalloc(sizeof(tftp_ctx_t));

    tftp_ctx->udp_socket_fd = udp_socket_fd;
    tftp_ctx->udp_socket = get_kernel_fd(udp_socket_fd);
    tftp_ctx->conn_created = false;

    return tftp_ctx;
}

static void tftp_send_read_req(tftp_ctx_t* ctx, const char* fname) {
    const int64_t name_len = strlen(fname);
    int64_t pkt_len = 2 + name_len + 1 + strlen("octet") + 1;

    uint8_t* pkt = vmalloc(pkt_len);
    memset(pkt, 0, pkt_len);

    // opcode
    pkt[0] = 0;
    pkt[1] = 1;
    memcpy(&pkt[2], fname, name_len);
    memcpy(&pkt[2 + name_len + 1], "octet", 5);

    for (int retry_count = 0; retry_count < 10; retry_count++) {
        int64_t bwrite = fd_call_write(ctx->udp_socket, pkt, pkt_len, 0);
        if (bwrite < 0) {
            task_wait_timer_in(1000*1000);
            continue;
        }
        ASSERT(bwrite == pkt_len);
        break;
    }

    vfree(pkt);
}

static void tftp_send_ack(tftp_ctx_t* ctx, uint16_t block_num) {

    uint8_t pkt[4];
    pkt[0] = 0;
    pkt[1] = 4;
    pkt[2] = block_num >> 8;
    pkt[3] = block_num & 0xFF;

    int64_t bwrite = fd_call_write(ctx->udp_socket, pkt, 4, 0);
    (void)bwrite;
    //ASSERT(bwrite == 4);
}

static void tftp_init_xfer(tftp_ctx_t* ctx, const char* fname) {

    while (true) {
        tftp_send_read_req(ctx, fname);

        syscall_select_ctx_t ack_wait = {
            .fd = ctx->udp_socket_fd,
            .ready_mask = FD_READY_GEN_READ
        };

        uint64_t ready_mask_out;
        int64_t fd = select_wait(&ack_wait, 1, TFTP_TIMEOUT_US, &ready_mask_out);

        console_log(LOG_DEBUG, "TFTP Init Wait Complete %d", fd);

        if (fd == ctx->udp_socket_fd) {
            if (!ctx->conn_created) {
                k_socket_msginfo_t msginfo;
                int64_t ok = fd_call_ioctl_ptr(ctx->udp_socket, SOCKET_IOCTL_GET_MSGINFO, &msginfo);
                ASSERT(ok == 0);

                k_socket_config_t socket_cfg;
                socket_cfg.socket_type = SYSCALL_SOCKET_UDP4;
                socket_cfg.udp4.dest_port = msginfo.udp4.source_port;
                socket_cfg.udp4.flags = K_SOCKET_CONFIG_DEST_PORT;
                ok = fd_call_ioctl_ptr(ctx->udp_socket, SOCKET_IOCTL_SET_CONFIG, &socket_cfg);
                ASSERT(ok == 0);

                ctx->conn_created = true;
            }

            break;
        }
    }
}

static int64_t tftp_recv_data(tftp_ctx_t* ctx, uint8_t* data, int64_t block_num) {

    while (true) {

        syscall_select_ctx_t pkt_wait = {
            .fd = ctx->udp_socket_fd,
            .ready_mask = FD_READY_GEN_READ
        };

        uint64_t ready_mask_out;
        int64_t fd = select_wait(&pkt_wait, 1, TFTP_TIMEOUT_US, &ready_mask_out);

        if (fd == ctx->udp_socket_fd) {

            int64_t brecv = fd_call_read(ctx->udp_socket, data, 516, 0);
            ASSERT(brecv > 0);

            if (data[0] != 0 ||
                data[1] == 5) {
                console_log(LOG_INFO, "Bad header %d %d", data[0], data[1]);
                
                continue;
            }

            if (brecv >= 4 &&
                data[1] == 3 &&
                data[2] == (block_num >> 8) &&
                data[3] == (block_num & 0xFF)) {
                tftp_send_ack(ctx, block_num);

                return brecv - 4;
            }
        // } else if (block_num != 1) {
        } else {
            console_log(LOG_INFO, "TFTP Resend Ack");
            // Resend previous ack
            tftp_send_ack(ctx, block_num - 1);
        }
    }
}

int64_t tftp_recv_file(tftp_ctx_t* ctx, const char* fname, void** file_data, int64_t* file_len) {

    console_log(LOG_DEBUG, "TFTP Init");

    tftp_init_xfer(ctx, fname);

    console_log(LOG_DEBUG, "TFTP Start Data");

    // llist_head_t chunks = llist_create();

    uint8_t** chunks = vmalloc(8192 * sizeof(void*));
    memset(chunks, 0, 8102 * sizeof(void*));

    int64_t block_num = 1;
    int64_t file_size = 0;
    bool abort = false;
    int64_t chunk_idx = 0;
    while (true) {
        // uint8_t* data = vmalloc(512 + 4);
        uint8_t* data = vmalloc(1024);


        int64_t bytes = tftp_recv_data(ctx, data, block_num);
        vmalloc_check_structure();

        chunks[chunk_idx++] = data;
        ASSERT(chunk_idx < 8192);
        block_num++;
        file_size += bytes;

        if (file_size % (250*1024) == 0) {
            console_log(LOG_DEBUG, "TFTP Recv %d KB", file_size / 1024);
        }


        if (bytes < 0) {
            abort = true;
            break;
        }

        if (bytes < 512) {
            break;
        }
    }

    ASSERT(!abort);

    uint8_t* tmp_file_data = vmalloc(file_size);
    *file_len = file_size;
    int64_t file_idx = 0;

    uint8_t* data;
    /*
    FOR_LLIST(chunks, data)
        if (file_size - idx >= 512) {
            memcpy(&tmp_file_data[idx], &data[4], 512);
        } else {
            memcpy(&tmp_file_data[idx], &data[4], file_size - idx);
        }
        idx += 512;
        // vfree(data);
    END_FOR_LLIST()

    // llist_free_all(chunks);
    // llist_free(chunks);
    */

    vmalloc_check_structure();
    for (int idx = 0; idx < 8192; idx++) {
        if (chunks[idx] == 0) {
            break;
        }
        // console_log(LOG_DEBUG, "Chunk %d %16x", idx, chunks[idx]);
        data = chunks[idx];

        if (file_size - file_idx >= 512) {
            memcpy(&tmp_file_data[file_idx], &data[4], 512);
        } else {
            console_log(LOG_DEBUG, "Copy %d", file_size - file_idx);
            memcpy(&tmp_file_data[file_idx], &data[4], file_size - file_idx);
        }
        file_idx += 512;

        // vmalloc_check_structure();
        // vfree(chunks[idx]);
    }
    // vfree(chunks);

    *file_data = tmp_file_data;
    return 0;
}
