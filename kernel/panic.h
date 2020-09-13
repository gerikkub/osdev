#ifndef __PANIC_H__
#define __PANIC_H__

#define PANIC(x) panic(__FILE__, __LINE__, x)

void panic(char* file, uint64_t line, char* msg, ...);

#endif
