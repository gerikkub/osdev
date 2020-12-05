
#include "kernel/task.h"
#include "kernel/console.h"
#include "kernel/assert.h"

#include "stdlib/printf.h"

void panic(char* file, uint64_t line, char* msg, ...) {
    console_printf("Panic: %s:%u %s\n", file, line, msg);

    //asm ("brk #0");
    while (1) {
    }
}

void task_panic(char* file, uint64_t line, char* msg, ...) {
    task_t* t = get_active_task();
    ASSERT(t != NULL);

    console_printf("Task Panic: %s:%u %s\n", file, line, msg);
    console_printf("Task: %d\n", t->tid);

    while (1) {
    }
}