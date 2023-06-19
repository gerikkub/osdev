
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

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
#include "stdlib/string.h"

int64_t main(uint64_t tid, char** ctx) {

    if (ctx[0] == NULL ||
        ctx[1] == NULL) {

        console_printf("Invalid Arguments\n");
        return -1;
    }

    char* ip_str = ctx[0];
    uint16_t listen_port = strtoi64(ctx[1], NULL);

    char* ip_str_0 = ip_str;
    char* ip_str_1 = ip_str;
    char* ip_str_2 = ip_str;
    char* ip_str_3 = ip_str;

    for (ip_str_1 = ip_str; *ip_str_1 != '.'; ip_str_1++) {
        if (*ip_str_1 == '\0') {
            console_printf("Invalid Arguments\n");
            return -1;
        }
    }
    *ip_str_1 = '\0';
    ip_str_1++;

    for (ip_str_2 = ip_str_1; *ip_str_2 != '.'; ip_str_2++) {
        if (*ip_str_2 == '\0') {
            console_printf("Invalid Arguments\n");
            return -1;
        }
    }
    *ip_str_2 = '\0';
    ip_str_2++;


    for (ip_str_3 = ip_str_2; *ip_str_3 != '.'; ip_str_3++) {
        if (*ip_str_3 == '\0') {
            console_printf("Invalid Arguments\n");
            return -1;
        }
    }
    *ip_str_3 = '\0';
    ip_str_3++;

    k_ipv4_t ip;
    ip.d[0] = strtoi64(ip_str_0, NULL);
    ip.d[1] = strtoi64(ip_str_1, NULL);
    ip.d[2] = strtoi64(ip_str_2, NULL);
    ip.d[3] = strtoi64(ip_str_3, NULL);

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

    while (true) {
        int64_t socket_fd = system_ioctl(bind_fd, BIND_IOCTL_GET_INCOMING, NULL, 0);

        if (socket_fd < 0) {
            system_yield();
            continue;
        }

        console_printf("New connection\n");
        console_flush();

        uint8_t buffer[256];
        int64_t bytes_read;
        do {
            memset(buffer, 0, 256);
            bytes_read = system_read(socket_fd, buffer, 255, K_SOCKET_READ_FLAGS_NONBLOCKING);

            if (bytes_read > 0) {
                console_printf("Read: %s\n", buffer);
                console_flush();
            }
            system_write(socket_fd, buffer, bytes_read, 0);

        } while (bytes_read >= 0);

        system_close(socket_fd);

        console_printf("Connection closed\n");
        console_flush();
    }

    return 0;
}
