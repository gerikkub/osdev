
#ifndef __IOCTL_COMMON_H__
#define __IOCTL_COMMON_H__

#define IOCTL_SEEK 0
#define IOCTL_SEEK_END 1

// Bulk Ops
#define BLK_IOCTL_SIZE 32

// Net Ops
#define NET_IOCTL_SET_IP 64
#define NET_IOCTL_GET_IP 65
#define NET_IOCTL_SET_ROUTE 66
#define NET_IOCTL_SET_DEFAULT_ROUTE 67

// Bind Ops
#define BIND_IOCTL_GET_INCOMING 64

#endif
