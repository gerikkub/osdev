
#include "kernel/console.h"
#include "stdlib/printf.h"

void panic(char* file, uint64_t line, char* msg) {
    /*console_write("Panic: ");
    console_write(file);
    console_write(":");
    console_write_num(line);
    console_write(" ");
    console_write(msg);
    console_endl();*/
    console_printf("Panic: %s:%u %s\n", file, line, msg);

    //asm ("brk #0");
    while (1) {
    }
}