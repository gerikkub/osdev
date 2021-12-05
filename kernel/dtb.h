#ifndef __DTB_H__
#define __DTB_H__

#include "kernel/lib/libdtb.h"

void dtb_init(void);

const dt_block_t* dtb_get_devicetree(void);

#endif