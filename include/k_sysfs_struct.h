
#ifndef __K_SYSFS_STRUCT_H__
#define __K_SYSFS_STRUCT_H__

typedef struct __attribute__((__packed__)) {
    uint64_t time_s;
    uint64_t time_us;
} ksysfs_struct_time_raw_t;



#endif