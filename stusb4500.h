#pragma once

#include <stdbool.h>
#include <stdint.h>

#define STUSB4500_ENABLE_PRINTF

enum {
    STUSB4500_GPIO_STATE_HIZ = 0UL,
    STUSB4500_GPIO_STATE_LOW = 1UL,
};
typedef uint8_t stusb4500_gpio_state_t;

typedef uint16_t stusb4500_current_t;
typedef uint16_t stusb4500_voltage_t;
typedef uint32_t (*stusb4500_get_ms_func_t)(void);

typedef struct {
    stusb4500_current_t min_current_ma;
    stusb4500_voltage_t min_voltage_mv;
    stusb4500_voltage_t max_voltage_mv;
    stusb4500_get_ms_func_t get_ms;
} stusb4500_config_t;

bool stusb4500_negotiate(stusb4500_config_t* config, bool on_interrupt);
bool stusb4500_set_gpio_state(stusb4500_gpio_state_t state);
