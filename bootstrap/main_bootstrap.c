
#include <stdint.h>
#include <stdbool.h>

#include "bootstrap/vmem_bootstrap.h"

void _high_start(void);

void main_bootstrap(void) __attribute__((section (".bootstrap.text")));

void bootstrap_write_byte(uint8_t b) __attribute__((section (".bootstrap.text")));
void bootstrap_write_word(uint64_t w) __attribute__((section (".bootstrap.text")));

void bootstrap_write_byte(uint8_t b) {
    volatile uint8_t* uart_tx_ptr = (volatile uint8_t*)0x47E215040;
    // volatile uint8_t* uart_tx_ptr = (volatile uint8_t*)0xFE215040;
    *uart_tx_ptr = b;
    for (volatile uint64_t idx = 1000; idx > 0; idx--);
}

void bootstrap_write_word(uint64_t w) {
    char hexmap[16];
    hexmap[0] = '0';
    hexmap[1] = '1';
    hexmap[2] = '2';
    hexmap[3] = '3';
    hexmap[4] = '4';
    hexmap[5] = '5';
    hexmap[6] = '6';
    hexmap[7] = '7';
    hexmap[8] = '8';
    hexmap[9] = '9';
    hexmap[10] = 'a';
    hexmap[11] = 'b';
    hexmap[12] = 'c';
    hexmap[13] = 'd';
    hexmap[14] = 'e';
    hexmap[15] = 'f';

    for (int64_t idx = 60; idx >=0; idx -= 4) {
        uint64_t val = (w >> idx) & 0xF;
        bootstrap_write_byte(hexmap[val]);
    }
}

void main_bootstrap(void) {

    setup_vmem_bootstrap();

    //while (1);
    _high_start();

    while (1); // Should not reach
}

