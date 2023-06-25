
#include <string.h>
#include <stdlib.h>

#include "stdlib/bitutils.h"

#include <k_net_api.h>

bool parse_ipv4_str(const char* ip_str, k_ipv4_t* ipv4) {

    uint64_t ip_strlen = strlen(ip_str) + 1;
    char* ip_str_copy = malloc(ip_strlen);
    memcpy(ip_str_copy, ip_str, ip_strlen);
    ip_str_copy[ip_strlen-1] = '\0';

    char* ip_str_0 = ip_str_copy;
    char* ip_str_1 = ip_str_copy;
    char* ip_str_2 = ip_str_copy;
    char* ip_str_3 = ip_str_copy;

    for (ip_str_1 = ip_str_copy; *ip_str_1 != '.'; ip_str_1++) {
        if (*ip_str_1 == '\0') {
            return false;
        }
    }
    *ip_str_1 = '\0';
    ip_str_1++;

    for (ip_str_2 = ip_str_1; *ip_str_2 != '.'; ip_str_2++) {
        if (*ip_str_2 == '\0') {
            return false;
        }
    }
    *ip_str_2 = '\0';
    ip_str_2++;


    for (ip_str_3 = ip_str_2; *ip_str_3 != '.'; ip_str_3++) {
        if (*ip_str_3 == '\0') {
            return false;
        }
    }
    *ip_str_3 = '\0';
    ip_str_3++;

    ipv4->d[0] = strtol(ip_str_0, NULL, 10);
    ipv4->d[1] = strtol(ip_str_1, NULL, 10);
    ipv4->d[2] = strtol(ip_str_2, NULL, 10);
    ipv4->d[3] = strtol(ip_str_3, NULL, 10);

    return true;
}