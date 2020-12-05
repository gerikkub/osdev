

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "include/k_syscall.h"

#include "system/system_console.h"
#include "system/system_assert.h"
#include "system/system_lib.h"

#define CONSOLE_BUFFER_SIZE 512

static char s_console_buffer[CONSOLE_BUFFER_SIZE+1];
static uint64_t s_console_idx = 0;

void console_putc(const char c) {
    SYS_ASSERT(s_console_idx < CONSOLE_BUFFER_SIZE);

    s_console_buffer[s_console_idx] = c;
    s_console_idx++;
    if (s_console_idx == CONSOLE_BUFFER_SIZE) {
        console_flush();
    }
}

void console_endl(void) {
    console_putc('\n');
}

void console_write(const char* s) {
    uint64_t len = strlen(s);

    console_write_len(s, len);
}

void console_write_len(const char* s, uint64_t len) { 
    SYS_ASSERT(s_console_idx < CONSOLE_BUFFER_SIZE);

    uint64_t space = CONSOLE_BUFFER_SIZE - s_console_idx;
    
    while (len > space) {
        strncpy(&s_console_buffer[s_console_idx], s, space);
        console_flush();
        s += space;
        len -= space;
        space = CONSOLE_BUFFER_SIZE - s_console_idx;
    }

    strncpy(&s_console_buffer[s_console_idx], s, len);
    s_console_idx += len;

    if (s_console_idx == CONSOLE_BUFFER_SIZE) {
        console_flush();
    }
}

void console_flush(void) {
    SYS_ASSERT(s_console_idx <= CONSOLE_BUFFER_SIZE);

    s_console_buffer[s_console_idx] = '\0';

    SYSCALL_CALL(SYSCALL_PRINT, (uintptr_t)s_console_buffer, 0, 0, 0);
    s_console_idx = 0;
}