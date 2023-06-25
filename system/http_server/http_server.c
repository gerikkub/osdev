
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

enum {
    HTTP_METHOD_GET = 0,
    HTTP_METHOD_POST,
    HTTP_METHOD_PUT
};

static const char* s_method_strs[] = {"GET", "POST", "PUT"};

typedef struct {
    int64_t method;
    char* url;
} http_request_t;

enum {
    HTTP_STATUS_OK = 0,
    HTTP_STATUS_NOT_FOUND
};

//static const char* s_status_strs[] = {"OK", "Not Found"};

typedef struct {
    int64_t status;
    uint64_t content_len;
    const void* payload;
} http_response_t;

const char* s_http_404_response = \
"HTTP/1.1 404 Not Found\r\n\r\n";

const char* s_http_200_response = \
"HTTP/1.1 200 OK\r\n"
"Content-Length: %u\r\n"
"Content-Type: text/html\r\n\r\n%s";


const char* s_html_body = \
"<!DOCTYPE html>"
"<html>"
"<head>"
"<title>Page Title</title>"
"</head>"
"<body>"
""
"<form action=\"/S\">"
"  <label for=\"fname\">Credit Card Info (Or SSN):</label><br>"
"  <input type=\"text\" id=\"fname\" name=\"fname\" value=\"Enter CC number here\"><br>"
"  <input type=\"submit\" value=\"submit\">"
"</form>"
"</body>"
"</html>";

const char* s_html_body_submit = \
"<!DOCTYPE html>"
"<html>"
"<head>"
"<title>Page Title</title>"
"</head>"
"<body>"
""
"<p>Thank you. Your information will be kept securly on the server</p>"
"</body>"
"</html>";

/*
const char* s_html_body = \
"<!DOCTYPE html>"
"<html>"
"<head>"
"<title>Page Title</title>"
"</head>"
"<body>"
""
"<h1>This is a Heading</h1>"
"<p>This is a paragraph.</p>"
""
"</body>"
"</html>";
*/


bool parse_http(char* http_buffer, uint64_t len, http_request_t* req) {

    const int64_t num_methods = sizeof(s_method_strs) / sizeof(s_method_strs[0]);
    int64_t idx;

    for (idx = 0; idx < num_methods; idx++) {
        if (memcmp(s_method_strs[idx], http_buffer, strlen(s_method_strs[idx])) == 0) {
            req->method = idx;
            break;
        }
    }

    if (idx == num_methods) {
        return false;
    }

    char* buffer_copy = malloc(len + 1 + 4096);
    memset(buffer_copy, 0, len+1);
    memcpy(buffer_copy, http_buffer, len);

    char* strtok_state;
    strtok_r(buffer_copy, " ", &strtok_state);

    char* url_str = strtok_r(NULL, " ", &strtok_state);
    uint64_t url_str_len = strlen(url_str);

    req->url = malloc(url_str_len + 1 + 4096);
    memcpy(req->url, url_str, url_str_len);
    req->url[url_str_len] = '\0';

    free(buffer_copy);

    return true;
}

void process_http_request(http_request_t* request, http_response_t* response) {
    if (request->method != HTTP_METHOD_GET) {
        response->status = HTTP_STATUS_NOT_FOUND;
        response->content_len = 0;
        response->payload = NULL;
        return;
    }

    if (memcmp(request->url, "/\0", 2) == 0) {
        response->status = HTTP_STATUS_OK;
        response->content_len = strlen(s_html_body);
        response->payload = s_html_body;
    } else if (memcmp(request->url, "/S\?", 3) == 0) {
        response->status = HTTP_STATUS_OK;
        response->content_len = strlen(s_html_body_submit);
        response->payload = s_html_body_submit;

        char* url_copy = malloc(strlen(request->url) + 1 + 4096);
        memset(url_copy, 0, strlen(request->url) + 1);
        memcpy(url_copy, request->url, strlen(request->url));
        char* strtok_state;
        strtok_r(url_copy, "=", &strtok_state);
        char* form_str = strtok_r(NULL, "=", &strtok_state);

        console_printf("You typed: %s\n", form_str);
        console_flush();

        free(url_copy);

    } else {
        response->status = HTTP_STATUS_NOT_FOUND;
        response->content_len = 0;
        response->payload = NULL;
    }

}

void send_http_response(int64_t socket_fd, http_response_t* response) {

    char* response_str = malloc(4096 + 4096);
    memset(response_str, 0, 4096);
    int64_t response_len = 0;

    if (response->status == HTTP_STATUS_NOT_FOUND) {
        response_len = snprintf(response_str,
                                4095, s_http_404_response);
    } else {
        response_len = snprintf(response_str, 4095,
                                s_http_200_response,
                                response->content_len,
                                response->payload);
    }

    //console_printf("Sending Response\n%s", response_str);
    //console_flush();

    int64_t bytes_sent = 0;
    int64_t total_bytes_sent = 0;

    while (total_bytes_sent < response_len) {
        bytes_sent = system_write(socket_fd, &response_str[total_bytes_sent], response_len - total_bytes_sent, 0);

        if (bytes_sent < 0) {
            break;
        }

        total_bytes_sent += bytes_sent;
    }

    free(response_str);
}

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

    while (true) {
        int64_t socket_fd = system_ioctl(bind_fd, BIND_IOCTL_GET_INCOMING, NULL, 0);

        if (socket_fd < 0) {
            system_yield();
            continue;
        }

        console_printf("New connection\n");
        console_flush();

        char* http_buffer = malloc(4096 + 4096);
        int64_t bytes_read;
        int64_t total_bytes_read;
        int64_t end_idx;
        do {
            memset(http_buffer, 0, 4096);
            total_bytes_read = 0;
            end_idx = 0;

            while (true) {
                bytes_read = system_read(socket_fd, &http_buffer[total_bytes_read], 4095-total_bytes_read, K_SOCKET_READ_FLAGS_NONBLOCKING);
                if (bytes_read < 0) {
                    break;
                }

                if (bytes_read > 0) {
                }

                if (bytes_read > 0) {
                    total_bytes_read += bytes_read;

                    //console_printf("Total Read: %d\n%s", total_bytes_read, http_buffer);
                    //console_flush();

                    for (int64_t idx = 0; idx < total_bytes_read; idx++) {
                        if(memcmp(&http_buffer[idx], "\r\n\r\n", 4) == 0) {
                            end_idx = idx + 4;
                            break;
                        }
                    }
                }

                if (end_idx > 0 || bytes_read < 0) {
                    break;
                }
            }
            
            if (bytes_read < 0) {
                break;
            }

            http_request_t request;
            http_response_t response;
            bool parse_ok;
            parse_ok = parse_http(http_buffer, end_idx, &request);
            if (!parse_ok) {
                console_printf("Unable to parse http: %s\n", http_buffer);
                console_flush();
                continue;
            }

            process_http_request(&request, &response);

            send_http_response(socket_fd, &response);

        } while (true);

        system_close(socket_fd);

        console_printf("Connection closed\n");
        console_flush();
    }

    return 0;
}
