
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <string_utils.h>

#include "system/lib/system_lib.h"
#include "system/lib/system_assert.h"
#include "system/lib/system_msg.h"
#include "system/lib/system_console.h"
#include "system/lib/system_assert.h"
#include "system/lib/system_file.h"
#include "system/lib/system_malloc.h"
#include "system/lib/system_socket.h"

#include "include/k_syscall.h"
#include "include/k_select.h"
#include "include/k_messages.h"
#include "include/k_modules.h"
#include "include/k_ioctl_common.h"
#include "include/k_net_api.h"

#include "stdlib/printf.h"

int64_t main(uint64_t tid, char** ctx) {

    if (ctx[0] == NULL ||
        ctx[1] == NULL) {

        console_printf("Invalid Arguments\n");
        return -1;
    }

    char* ip_str = ctx[0];
    uint16_t listen_port = strtoll(ctx[1], NULL, 10);
    k_ipv4_t ip;

    bool ok = parse_ipv4_str(ip_str, &ip);
    if (!ok) {
        console_printf("Invalid Arguments\n");
        return -1;
    }

    k_bind_port_t bind_setup = {
        .bind_type = SYSCALL_BIND_TCP4,
        .tcp4.bind_ip = ip,
        .tcp4.listen_port = listen_port
    };

    int64_t bind_fd = -1;
    bind_fd = system_bind(&bind_setup);

    if (bind_fd < 0) {
        console_printf("Unable to bind port\n");
        return -1;
    }

    syscall_select_ctx_t* select_arr = malloc(sizeof(syscall_select_ctx_t) * 33);

    for (uint64_t idx = 0; idx < 33; idx++) {
        select_arr[idx].fd = -1;
        select_arr[idx].ready_mask = 0;
    }

    select_arr[0].fd = bind_fd;
    select_arr[0].ready_mask = FD_READY_BIND_NEWCONN;

    while (true) {
        uint64_t ready_op;

        int64_t ready_fd = system_select(select_arr, 33, UINT64_MAX, &ready_op);

        console_printf("Ready FD: %d (%d)\n", ready_fd, ready_op);
        console_flush();

        if (ready_fd == bind_fd) {
            int64_t socket_fd = system_ioctl(bind_fd, BIND_IOCTL_GET_INCOMING, NULL, 0);

            uint64_t idx;
            for (idx = 0; idx < 33; idx++) {
                if (select_arr[idx].fd == -1) {
                    break;
                }
            }
            SYS_ASSERT(idx < 33);

            select_arr[idx].fd = socket_fd;
            select_arr[idx].ready_mask = FD_READY_GEN_READ | FD_READY_GEN_CLOSE;
        } else {
            if (ready_op & FD_READY_GEN_READ) {
                uint8_t buffer[256];
                int64_t bytes_read;

                memset(buffer, 0, 256);
                bytes_read = system_read(ready_fd, buffer, 255, 0);

                system_write(ready_fd, buffer, bytes_read, 0);
            }
            if (ready_op & FD_READY_GEN_CLOSE) {
                system_close(ready_fd);

                for (uint64_t idx = 0; idx < 33; idx++) {
                    if (select_arr[idx].fd == ready_fd) {
                        select_arr[idx].fd = -1;
                        break;
                    }
                }
            }
        }
    }

    return 0;
}
