/*
 * CH422G I2C GPIO Expander Driver Implementation
 */

#include "ch422g.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_err.h"
#include <string.h>

static const char *TAG = "CH422G";
static i2c_master_bus_handle_t ch422g_bus_handle;
static uint8_t ch422g_device_addr = CH422G_I2C_ADDR;
static i2c_master_dev_handle_t ch422g_dev_handle;

/**
 * @brief Initialize CH422G device
 *
 * @param bus_handle I2C master bus handle
 * @param addr I2C device address (default 0x24)
 * @return esp_err_t ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ch422g_init(i2c_master_bus_handle_t bus_handle, uint8_t addr)
{
    ch422g_bus_handle = bus_handle;
    ch422g_device_addr = addr;

    // Configure I2C device
    i2c_device_config_t dev_cfg = {
        .scl_speed_hz = 100000, // 100kHz
        .device_address = ch422g_device_addr,
    };

    esp_err_t ret = i2c_master_bus_add_device(ch422g_bus_handle, &dev_cfg, &ch422g_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add CH422G device to I2C bus");
        return ret;
    }

    ESP_LOGI(TAG, "CH422G initialized successfully at address 0x%02x", ch422g_device_addr);
    return ESP_OK;
}

/**
 * @brief Write a byte to CH422G
 *
 * @param value Byte to write
 * @return esp_err_t ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ch422g_write_byte(uint8_t value)
{
    uint8_t data[1] = {value};
    esp_err_t ret = i2c_master_transmit(ch422g_dev_handle, data, 1, 100);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write byte to CH422G");
    }
    return ret;
}

/**
 * @brief Read a byte from CH422G
 *
 * @param value Pointer to store the read byte
 * @return esp_err_t ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ch422g_read_byte(uint8_t *value)
{
    uint8_t data[1];
    esp_err_t ret = i2c_master_receive(ch422g_dev_handle, data, 1, 100);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read byte from CH422G");
        return ret;
    }
    *value = data[0];
    return ESP_OK;
}

/**
 * @brief Write to a specific register
 *
 * @param reg Register address
 * @param value Value to write
 * @return esp_err_t ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ch422g_write_register(uint8_t reg, uint8_t value)
{
    uint8_t data[2] = {reg, value};
    esp_err_t ret = i2c_master_transmit(ch422g_dev_handle, data, 2, 100);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write register 0x%02x to CH422G", reg);
    }
    return ret;
}

/**
 * @brief Read from a specific register
 *
 * @param reg Register address
 * @param value Pointer to store the read value
 * @return esp_err_t ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ch422g_read_register(uint8_t reg, uint8_t *value)
{
    esp_err_t ret = i2c_master_transmit_receive(ch422g_dev_handle, &reg, 1, value, 1, 100);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read register 0x%02x from CH422G", reg);
    }
    return ret;
}

/**
 * @brief Set pin direction (input/output)
 *
 * @param pin Pin number (0-7)
 * @param direction Direction (0=input, 1=output)
 * @return esp_err_t ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ch422g_set_direction(uint8_t pin, ch422g_direction_t direction)
{
    if (pin > 7) {
        ESP_LOGE(TAG, "Invalid pin number: %d", pin);
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t iodir_value;
    esp_err_t ret = ch422g_read_register(CH422G_REG_IODIR, &iodir_value);
    if (ret != ESP_OK) {
        return ret;
    }

    if (direction == CH422G_DIR_OUTPUT) {
        iodir_value |= (1 << pin);  // Set bit for output
    } else {
        iodir_value &= ~(1 << pin); // Clear bit for input
    }

    ret = ch422g_write_register(CH422G_REG_IODIR, iodir_value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set direction for pin %d", pin);
    }
    return ret;
}

/**
 * @brief Set pin value (for output pins)
 *
 * @param pin Pin number (0-7)
 * @param value Pin value (0=low, 1=high)
 * @return esp_err_t ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ch422g_set_pin(uint8_t pin, uint8_t value)
{
    if (pin > 7) {
        ESP_LOGE(TAG, "Invalid pin number: %d", pin);
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t odr_value;
    esp_err_t ret = ch422g_read_register(CH422G_REG_ODR, &odr_value);
    if (ret != ESP_OK) {
        return ret;
    }

    if (value == 1) {
        odr_value |= (1 << pin);  // Set bit
    } else {
        odr_value &= ~(1 << pin); // Clear bit
    }

    ret = ch422g_write_register(CH422G_REG_ODR, odr_value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set pin %d to %d", pin, value);
    } else {
        ESP_LOGD(TAG, "Set pin %d to %d", pin, value);
    }
    return ret;
}

/**
 * @brief Get pin value (for input pins)
 *
 * @param pin Pin number (0-7)
 * @param value Pointer to store pin value
 * @return esp_err_t ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ch422g_get_pin(uint8_t pin, uint8_t *value)
{
    if (pin > 7) {
        ESP_LOGE(TAG, "Invalid pin number: %d", pin);
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t idr_value;
    esp_err_t ret = ch422g_read_register(CH422G_REG_IDR, &idr_value);
    if (ret != ESP_OK) {
        return ret;
    }

    *value = (idr_value >> pin) & 0x01;
    ESP_LOGD(TAG, "Read pin %d value: %d", pin, *value);
    return ESP_OK;
}

/**
 * @brief Set output control (push-pull or open-drain)
 *
 * @param pin Pin number (0-7)
 * @param control Output control type
 * @return esp_err_t ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ch422g_set_output_control(uint8_t pin, ch422g_output_control_t control)
{
    if (pin > 7) {
        ESP_LOGE(TAG, "Invalid pin number: %d", pin);
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t ocon_value;
    esp_err_t ret = ch422g_read_register(CH422G_REG_OCON, &ocon_value);
    if (ret != ESP_OK) {
        return ret;
    }

    if (control == CH422G_OC_OPEN_DRAIN) {
        ocon_value |= (1 << pin);  // Set bit for open-drain
    } else {
        ocon_value &= ~(1 << pin); // Clear bit for push-pull
    }

    ret = ch422g_write_register(CH422G_REG_OCON, ocon_value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set output control for pin %d", pin);
    }
    return ret;
}

// LCD Pin definitions (adjust according to your hardware)
#define CH422G_LCD_BACKLIGHT_PIN 0  // LCD backlight control pin
#define CH422G_LCD_RESET_PIN     1  // LCD reset pin

/**
 * @brief Initialize LCD control pins
 *
 * @return esp_err_t ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ch422g_lcd_init(void)
{
    ESP_LOGI(TAG, "Initializing LCD control pins");

    // Set LCD pins as outputs
    esp_err_t ret = ch422g_set_direction(CH422G_LCD_BACKLIGHT_PIN, CH422G_DIR_OUTPUT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set LCD backlight pin direction");
        return ret;
    }

    ret = ch422g_set_direction(CH422G_LCD_RESET_PIN, CH422G_DIR_OUTPUT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set LCD reset pin direction");
        return ret;
    }

    // Set output control to push-pull
    ret = ch422g_set_output_control(CH422G_LCD_BACKLIGHT_PIN, CH422G_OC_PUSH_PULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set LCD backlight pin output control");
        return ret;
    }

    ret = ch422g_set_output_control(CH422G_LCD_RESET_PIN, CH422G_OC_PUSH_PULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set LCD reset pin output control");
        return ret;
    }

    // Turn on backlight by default
    ret = ch422g_lcd_backlight_on();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to turn on LCD backlight");
        return ret;
    }

    // Release reset
    ret = ch422g_lcd_reset_off();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to release LCD reset");
        return ret;
    }

    ESP_LOGI(TAG, "LCD control pins initialized successfully");
    return ESP_OK;
}

/**
 * @brief Turn LCD backlight on
 *
 * @return esp_err_t ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ch422g_lcd_backlight_on(void)
{
    return ch422g_set_pin(CH422G_LCD_BACKLIGHT_PIN, 1);
}

/**
 * @brief Turn LCD backlight off
 *
 * @return esp_err_t ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ch422g_lcd_backlight_off(void)
{
    return ch422g_set_pin(CH422G_LCD_BACKLIGHT_PIN, 0);
}

/**
 * @brief Activate LCD reset (active low)
 *
 * @return esp_err_t ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ch422g_lcd_reset_on(void)
{
    return ch422g_set_pin(CH422G_LCD_RESET_PIN, 0);
}

/**
 * @brief Release LCD reset
 *
 * @return esp_err_t ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ch422g_lcd_reset_off(void)
{
    return ch422g_set_pin(CH422G_LCD_RESET_PIN, 1);
}

// SD Card Pin definitions (adjust according to your hardware)
#define CH422G_SD_CS_PIN 2  // SD card chip select pin

/**
 * @brief Initialize SD card control pins
 *
 * @return esp_err_t ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ch422g_sd_init(void)
{
    ESP_LOGI(TAG, "Initializing SD card control pins");

    // Set SD CS pin as output
    esp_err_t ret = ch422g_set_direction(CH422G_SD_CS_PIN, CH422G_DIR_OUTPUT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set SD CS pin direction");
        return ret;
    }

    // Set output control to push-pull
    ret = ch422g_set_output_control(CH422G_SD_CS_PIN, CH422G_OC_PUSH_PULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set SD CS pin output control");
        return ret;
    }

    // Set CS high (inactive) by default
    ret = ch422g_sd_cs_off();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set SD CS inactive");
        return ret;
    }

    ESP_LOGI(TAG, "SD card control pins initialized successfully");
    return ESP_OK;
}

/**
 * @brief Activate SD card chip select (low active)
 *
 * @return esp_err_t ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ch422g_sd_cs_on(void)
{
    return ch422g_set_pin(CH422G_SD_CS_PIN, 0);
}

/**
 * @brief Deactivate SD card chip select
 *
 * @return esp_err_t ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ch422g_sd_cs_off(void)
{
    return ch422g_set_pin(CH422G_SD_CS_PIN, 1);
}

// SD card manager integration variables
static bool sd_cs_active_low = true;  // SD card CS is typically active low
static uint8_t sd_cs_pin = CH422G_SD_CS_PIN;

/**
 * @brief Set SD card CS active low behavior
 *
 * @param active_low true for active low, false for active high
 */
void ch422g_sd_cs_set_active_low(bool active_low)
{
    sd_cs_active_low = active_low;
    ESP_LOGD(TAG, "SD CS active low set to: %s", active_low ? "true" : "false");
}

/**
 * @brief Get SD card CS active low behavior
 *
 * @return true if active low, false if active high
 */
bool ch422g_sd_cs_is_active_low(void)
{
    return sd_cs_active_low;
}

/**
 * @brief Set SD card CS pin number
 *
 * @param pin Pin number (0-7) for CH422G
 */
void ch422g_sd_cs_set_pin(uint8_t pin)
{
    if (pin <= 7) {
        sd_cs_pin = pin;
        ESP_LOGD(TAG, "SD CS pin set to: %d", pin);
    } else {
        ESP_LOGE(TAG, "Invalid SD CS pin: %d (must be 0-7)", pin);
    }
}

/**
 * @brief Get SD card CS pin number
 *
 * @return Pin number (0-7) for CH422G
 */
uint8_t ch422g_sd_cs_get_pin(void)
{
    return sd_cs_pin;
}

/**
 * @brief Set SD card CS pin value with proper polarity handling
 *
 * @param pin Pin number
 * @param value 0 or 1
 * @return esp_err_t ESP_OK on success, ESP_FAIL on error
 */
esp_err_t ch422g_sd_set_cs_pin_value(uint8_t pin, uint8_t value)
{
    // Apply polarity inversion if active low
    uint8_t actual_value = value;
    if (sd_cs_active_low) {
        actual_value = !value;
    }

    ESP_LOGD(TAG, "SD CS pin %d: logical %d -> physical %d", pin, value, actual_value);
    return ch422g_set_pin(pin, actual_value);
}

