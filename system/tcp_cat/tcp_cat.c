
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <string_utils.h>

#include "system/lib/system_lib.h"
#include "system/lib/system_msg.h"
#include "system/lib/system_console.h"
#include "system/lib/system_assert.h"
#include "system/lib/system_file.h"
#include "system/lib/system_malloc.h"
#include "system/lib/system_socket.h"

#include "include/k_syscall.h"
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

    char *fbuffer = malloc(256);
    while (true) {
        int64_t socket_fd = system_ioctl(bind_fd, BIND_IOCTL_GET_INCOMING, NULL, 0);

        if (socket_fd < 0) {
            system_yield();
            continue;
        }

        char buffer[256];
        int64_t bytes_read;
        memset(buffer, 0, 256);

        bytes_read = system_read(socket_fd, buffer, 255, 0);

        char* strtok_ctx;
        char* devname = strtok_r(buffer, " ", &strtok_ctx);
        char* fname = strtok_r(NULL, " ", &strtok_ctx);

        if (bytes_read > 0 && fname != NULL && devname != NULL) {

            int64_t file_fd = system_open(devname, fname, 0);

            if (file_fd >= 0) {
                int64_t fread_len;
                do {
                    fread_len = system_read(file_fd, fbuffer, 256, 0);
                    if (fread_len > 0) {
                        system_write(socket_fd, fbuffer, fread_len, 0);
                    }
                } while (fread_len > 0);

                system_close(file_fd);
            } else {
                console_printf("TCP Cat File not found: %s:%s", devname, fname);
                console_flush();
            }
        }

        system_close(socket_fd);

    }

    return 0;
}
