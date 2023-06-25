
#ifndef __STRING_UTILS_H__
#define __STRING_UTILS_H__

#include <stdbool.h>
#include <string.h>
#include <k_net_api.h>

bool parse_ipv4_str(const char* ip_str, k_ipv4_t* ipv4);

#endif
