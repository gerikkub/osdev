
#include <stdint.h>
#include <string.h>

#include "system/lib/system_lib.h"
#include "system/lib/system_msg.h"
#include "system/lib/system_console.h"
#include "system/lib/system_assert.h"
#include "system/lib/system_file.h"
#include "system/lib/system_malloc.h"

#include "include/k_syscall.h"
#include "include/k_messages.h"
#include "include/k_modules.h"
#include "include/k_ioctl_common.h"

void parse_prompt(char* inbuf, uint64_t len) {

    char* device = NULL;
    char* path = NULL;
    char* name = NULL;
    char** argv = NULL;

    uint64_t idx = 0;
    device = inbuf;
    while (idx < len) {
        if (inbuf[idx] == ':') {
            inbuf[idx] = '\0';
            break;
        }
        idx++;
    }

    if (idx == len) {
        return;
    }

    idx++;
    path = &inbuf[idx];
    while (idx < len) {
        if (inbuf[idx] == '\0' ||
            inbuf[idx] == ' ') {
            inbuf[idx] = '\0';
            break;
        } else if (inbuf[idx] == '/') {
            if (inbuf[idx+1] == ' ' ||
                inbuf[idx+1] == '\0') {
                return;
            }
            name = &inbuf[idx+1];
        }

        idx++;
    }

    if (idx != len) {
        argv = malloc(256 * sizeof(char*));
        uint64_t argc = 0;

        idx++;

        while (idx < len) {
            argv[argc] = &inbuf[idx];

            while (inbuf[idx] != '\0'&& 
                   inbuf[idx] != ' ') {
                idx++;
            }

            argc++;

            if (inbuf[idx] == ' ') {
                inbuf[idx] = '\0';
                idx++;
            } else {
                break;
            }

        }
        argv[argc] = NULL;
    }

    uint64_t tid = system_exec(device, path, name, argv);
    console_printf("\nLaunched %d", tid);
    console_flush();
}

void run_prompt() {
    console_printf("> ");
    console_flush();

    char inbuf[256];
    uint64_t count = 0;

    memset(inbuf, 0, sizeof(inbuf));
    while (1) {
        char c;
        console_read(&c, 1);
        console_putc(c);
        console_flush();
        if (c == '\r') {
            console_write("\n");
            parse_prompt(inbuf, count);

            count = 0;
            console_write("\n> ");
            console_flush();
            break;
        } else if (c == '\r') {
            count = 0;
            console_write("\n> ");
            console_flush();
        } else {
            inbuf[count] = c;
            count++;
            if (count > sizeof(inbuf)) {
                break;
            }
        }
    }

    console_printf("\n");
}

int32_t main(uint64_t tid, char** argv) {

    while (1) {
        run_prompt();
    }
}