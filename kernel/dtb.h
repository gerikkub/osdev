#ifndef __DTB_H__
#define __DTB_H__

#include "kernel/lib/libdtb.h"

void dtb_init(uintptr_t);
int dtb_query_name(const char* dev, discovery_dtb_ctx_t* disc_ctx);

#endif
