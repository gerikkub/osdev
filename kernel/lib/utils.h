
#pragma once

#define READ_MEM8(x) (*(volatile uint8_t*)(x))
#define READ_MEM16(x) (*(volatile uint16_t*)(x))
#define READ_MEM32(x) (*(volatile uint32_t*)(x))
#define READ_MEM64(x) (*(volatile uint64_t*)(x))

#define WRITE_MEM8(x, d) (*(volatile uint8_t*)(x)) = d
#define WRITE_MEM16(x, d) (*(volatile uint16_t*)(x)) = d
#define WRITE_MEM32(x, d) (*(volatile uint32_t*)(x)) = d
#define WRITE_MEM64(x, d) (*(volatile uint64_t*)(x)) = d
