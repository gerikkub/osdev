
#ifndef __K_GPIO_H__
#define __K_GPIO_H__

#include <stdint.h>

typedef struct {
    int64_t num;
    int64_t offset;
} k_gpio_props_t;

typedef struct {
    int64_t gpio_num;
    uint64_t flags;
    #define GPIO_CONFIG_FLAG_IN (0)
    #define GPIO_CONFIG_FLAG_OUT_PP BIT(0)
    #define GPIO_CONFIG_FLAG_OUT_OD BIT(1)
    #define GPIO_CONFIG_FLAG_OUT_MASK (BIT(0) | BIT(1))
    #define GPIO_CONFIG_FLAG_PULL_NONE (0)
    #define GPIO_CONFIG_FLAG_PULL_UP BIT(2)
    #define GPIO_CONFIG_FLAG_PULL_DOWN BIT(3)
    #define GPIO_CONFIG_FLAG_PULL_MASK (BIT(2) | BIT(3))
    #define GPIO_CONFIG_FLAG_AF_EN BIT(4)
    #define GPIO_CONFIG_FLAG_EV_NONE (0)
    #define GPIO_CONFIG_FLAG_EV_RISING BIT(5)
    #define GPIO_CONFIG_FLAG_EV_FALLING BIT(6)
    #define GPIO_CONFIG_FLAG_EV_HIGH BIT(7)
    #define GPIO_CONFIG_FLAG_EV_LOW BIT(8)
    uint64_t af;
} k_gpio_config_t;

typedef struct {
    int64_t gpio_num;
    int64_t level;
} k_gpio_level_t;

#endif
