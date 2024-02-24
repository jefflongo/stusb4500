#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef STUSB4500_LOG
#define STUSB4500_LOG(fmt, ...)
#endif // STUSB4500_LOG

typedef bool (*stusb4500_write_t)(
  uint16_t addr, uint8_t reg, void const* buf, size_t len, void* context);
typedef bool (*stusb4500_read_t)(uint16_t addr, uint8_t reg, void* buf, size_t len, void* context);
typedef uint32_t (*stusb4500_get_ms_func_t)(void);

typedef uint16_t stusb4500_current_t;
typedef uint16_t stusb4500_voltage_t;

enum {
    STUSB4500_GPIO_CFG_SW_CTRL = 0x00UL,
    STUSB4500_GPIO_CFG_ERROR_RECOVERY = 0x01UL,
    STUSB4500_GPIO_CFG_DEBUG = 0x02UL,
    STUSB4500_GPIO_CFG_SINK_POWER = 0x03UL,
};
typedef uint8_t stusb4500_gpio_cfg_t;

enum {
    STUSB4500_GPIO_STATE_HIZ = 0UL,
    STUSB4500_GPIO_STATE_LOW = 1UL,
};
typedef uint8_t stusb4500_gpio_state_t;

typedef struct {
    uint16_t addr;
    stusb4500_write_t write;
    stusb4500_read_t read;
    void* context;
} stusb4500_t;

typedef struct {
    stusb4500_current_t min_current_ma;
    stusb4500_voltage_t min_voltage_mv;
    stusb4500_voltage_t max_voltage_mv;
    stusb4500_get_ms_func_t get_ms;
} stusb4500_config_t;

typedef struct {
    // PDO1 voltage fixed to 5V
    stusb4500_current_t pdo1_current_ma;

    stusb4500_voltage_t pdo2_voltage_mv;
    stusb4500_current_t pdo2_current_ma;

    stusb4500_voltage_t pdo3_voltage_mv;
    stusb4500_current_t pdo3_current_ma;

    // This current is used if PDO current is 0
    stusb4500_current_t pdo_current_fallback;
    // 1, 2, or 3
    uint8_t num_valid_pdos;
    // Choose the source PDO's current if voltage matches
    bool use_src_current;
    // Do not fall back to 5V if no PDO match
    bool only_above_5v;
    // GPIO configuration. See stusb4500_gpio_cfg_t
    stusb4500_gpio_cfg_t gpio_cfg;
} stusb4500_nvm_config_t;

bool stusb4500_negotiate(
  stusb4500_t const* dev, stusb4500_config_t const* config, bool on_interrupt);
bool stusb4500_set_gpio_state(stusb4500_t const* dev, stusb4500_gpio_state_t state);

bool stusb4500_nvm_read(stusb4500_t const* dev, uint8_t* nvm);
bool stusb4500_nvm_flash(stusb4500_t const* dev, stusb4500_nvm_config_t const* config);
