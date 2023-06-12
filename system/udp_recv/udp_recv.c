
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
    uint16_t source_port = strtoi64(ctx[1], NULL);

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

    k_create_socket_t socket_setup = {
        .socket_type = SYSCALL_SOCKET_UDP4,
        .udp4.dest_ip = ip,
        .udp4.source_port = source_port,
        .udp4.dest_port = 0
    };

    int64_t socket_fd = -1;
    (void)socket_setup;
    socket_fd = system_socket(&socket_setup);

    if (socket_fd < 0) {
        console_printf("Unable to open socket\n");
        return -1;
    }

    uint8_t* packet = malloc(128);
    do {
        int64_t ret;
        ret = system_read(socket_fd, packet, 128, (1 << K_SOCKET_READ_FLAGS_NONBLOCKING));
        if (ret > 0) {
            console_printf("UDP_RECV message (%d) %s\n", ret, packet);
            console_flush();
        }
        system_yield();
    } while (true);

    return 0;
}