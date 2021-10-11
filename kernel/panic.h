#ifndef __PANIC_H__
#define __PANIC_H__

#include <stdint.h>

#define PANIC(x) panic(__FILE__, __LINE__, x)

void panic(char* file, uint64_t line, char* msg, ...);
void task_panic(char* file, uint64_t line, char* msg, ...);

#endif
