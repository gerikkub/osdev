
#include <stdint.h>
#include <stdbool.h>

#include "bootstrap/vmem_bootstrap.h"

void _high_start(void);

void main_bootstrap(void) __attribute__((section (".bootstrap.text")));

void main_bootstrap(void) {

    setup_vmem_bootstrap();

    _high_start();

    while (1); // Should not reach
}

