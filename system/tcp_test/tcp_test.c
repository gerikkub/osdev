
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
        ctx[1] == NULL ||
        ctx[2] == NULL ||
        ctx[3] == NULL) {

        console_printf("Invalid Arguments\n");
        return -1;
    }

    char* ip_str = ctx[0];
    uint16_t dest_port = strtoi64(ctx[1], NULL);
    char* msg = ctx[2];
    int64_t msg_len = strtoi64(ctx[3], NULL);

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
        .socket_type = SYSCALL_SOCKET_TCP4,
        .tcp4.dest_ip = ip,
        .tcp4.listen_port = 0,
        .tcp4.dest_port = dest_port
    };

    int64_t socket_fd = -1;
    (void)socket_setup;
    socket_fd = system_socket(&socket_setup);

    if (socket_fd < 0) {
        console_printf("Unable to open socket\n");
        return -1;
    }

    (void)msg;
    (void)msg_len;
    system_write(socket_fd, msg, msg_len, 0);

    while (true) {
        system_yield();
    }

    return 0;
}