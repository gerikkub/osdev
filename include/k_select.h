
#ifndef __K_SELECT_H__
#define __K_SELECT_H__

#include <stdint.h>

#include "stdlib/bitutils.h"

#define FD_READY_GEN_READ BIT(0)
#define FD_READY_GEN_WRITE BIT(1)
#define FD_READY_GEN_CLOSE BIT(2)
#define FD_READY_GEN_ATTENTION BIT(3)

// bind fds
#define FD_READY_BIND_NEWCONN BIT(8)

// task_ops fds
#define FD_READY_TASKOPS_WAIT BIT(8)

// gpio fds
#define FD_READY_GPIO_EVENT BIT(32)


#define FD_READY_ALL (UINT64_MAX)



#endif
