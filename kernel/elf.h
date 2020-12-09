
#ifndef __ELF_H__
#define __ELF_H__

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    ELF_VALID = 0,
    ELF_BADHEADER = 1,
    ELF_BADMAGIC = 2,
    ELF_BADIDENT = 3,
    ELF_CANTALLOC = 4,
    ELF_BADPHDR = 5,
    ELF_UNHANDLED_PHDR = 6,
    ELF_BADTASK = 7,
    ELF_EMPTYPHDR = 8,
} elf_result_t;


/** User/System Address Space
 * 
 * 0x0100_0000_0000  ________________
 *                  | System (RW/RX) |
 *    128 GB        | Libraries (RX) |
 * 0x00E0_0000_0000 |________________|
 *    128 GB        |  Stacks (RW)   |
 * 0x00C0_0000_0000 |________________|
 *    764 GB        |   Heap (RW)    |
 * 0x0001_0000_0000 |________________|
 *                  |    Bss (RW)    |
 *    2.5 GB        |----------------|
 *                  |   Data (RW)    |
 * 0x0000_6000_0000 |________________|
 *    512 MB        |   Text (RO)    |
 * 0x0000_4000_0000 |________________|
 *      1 GB        |    Reserved    |
 * 0x0000_0000_0000 |________________|
 **/

#define USER_ADDRSPACE_TOP (0x010000000000UL)
#define USER_ADDRSPACE_SYSTEM (0x00E000000000UL)
#define USER_ADDRSPACE_STACKTOP (0x00E000000000UL)
#define USER_ADDRSPACE_STACKLIMIT (0x00C000000000UL)
#define USER_ADDRSPACE_HEAP (0x000100000000UL)
#define USER_ADDRSPACE_DATA (0x000060000000UL)
#define USER_ADDRSPACE_TEXT (0x000040000000UL)
#define USER_ADDRSPACE_BASE (0UL)

#define USER_STACK_SIZE (8192UL)
#define USER_STACK_GUARD_PAGES (4096UL)

#define USER_GUARD_PAGES (4096UL)

uint64_t create_elf_task(uint8_t* elf_data, uint64_t elf_size, elf_result_t* result, bool system_task, char* name);

#define ELF_MAX_MEMSPACE_ENTRIES 128

#endif