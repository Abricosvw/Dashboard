/*
 * CH422G I2C GPIO Expander Driver
 * CH422G is an I2C GPIO expander with 8 GPIO pins
 */

#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// CH422G I2C Address (default)
#define CH422G_I2C_ADDR 0x24

// CH422G Register Addresses
#define CH422G_REG_OCON 0x18  // Output Control Register (OCON)
#define CH422G_REG_ODR  0x0A  // Output Data Register (ODR)
#define CH422G_REG_IDR  0x0C  // Input Data Register (IDR)
#define CH422G_REG_IODIR 0x00 // I/O Direction Register (IODIR)

// CH422G Pin definitions
typedef enum {
    CH422G_PIN_0 = 0,
    CH422G_PIN_1 = 1,
    CH422G_PIN_2 = 2,
    CH422G_PIN_3 = 3,
    CH422G_PIN_4 = 4,
    CH422G_PIN_5 = 5,
    CH422G_PIN_6 = 6,
    CH422G_PIN_7 = 7,
} ch422g_pin_t;

// CH422G Direction definitions
typedef enum {
    CH422G_DIR_INPUT = 0,
    CH422G_DIR_OUTPUT = 1,
} ch422g_direction_t;

// CH422G Output Control definitions
typedef enum {
    CH422G_OC_PUSH_PULL = 0,  // Push-pull output
    CH422G_OC_OPEN_DRAIN = 1, // Open-drain output
} ch422g_output_control_t;

// Function declarations
esp_err_t ch422g_init(i2c_master_bus_handle_t bus_handle, uint8_t addr);
esp_err_t ch422g_set_direction(uint8_t pin, ch422g_direction_t direction);
esp_err_t ch422g_set_pin(uint8_t pin, uint8_t value);
esp_err_t ch422g_get_pin(uint8_t pin, uint8_t *value);
esp_err_t ch422g_write_register(uint8_t reg, uint8_t value);
esp_err_t ch422g_read_register(uint8_t reg, uint8_t *value);
esp_err_t ch422g_write_byte(uint8_t value);
esp_err_t ch422g_read_byte(uint8_t *value);

// LCD control functions
esp_err_t ch422g_lcd_init(void);
esp_err_t ch422g_lcd_backlight_on(void);
esp_err_t ch422g_lcd_backlight_off(void);
esp_err_t ch422g_lcd_reset_on(void);
esp_err_t ch422g_lcd_reset_off(void);

// SD card control functions
esp_err_t ch422g_sd_init(void);
esp_err_t ch422g_sd_cs_on(void);  // Chip Select active
esp_err_t ch422g_sd_cs_off(void); // Chip Select inactive

// SD card manager integration functions
void ch422g_sd_cs_set_active_low(bool active_low);
bool ch422g_sd_cs_is_active_low(void);
void ch422g_sd_cs_set_pin(uint8_t pin);
uint8_t ch422g_sd_cs_get_pin(void);

#ifdef __cplusplus
}
#endif
