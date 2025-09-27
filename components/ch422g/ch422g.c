#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "ch422g.h"

static const char *TAG = "CH422G";

// CH422G Command Registers
#define CMD_SET_IO_DIR  0x48
#define CMD_SET_BIT_OUT 0x45
#define CMD_CLR_BIT_OUT 0x46
#define I2C_MASTER_TIMEOUT_MS 1000

struct ch422g_dev_t {
    i2c_master_dev_handle_t i2c_dev; // Modern I2C device handle
    uint16_t dir_cache;      // Cache for I/O direction registers
};

ch422g_handle_t ch422g_init(i2c_master_bus_handle_t bus_handle, uint8_t dev_addr)
{
    if (bus_handle == NULL) {
        ESP_LOGE(TAG, "I2C bus handle is NULL");
        return NULL;
    }

    ch422g_handle_t handle = (ch422g_handle_t)calloc(1, sizeof(struct ch422g_dev_t));
    if (handle == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for device handle");
        return NULL;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_7,
        .device_address = dev_addr,
        .scl_speed_hz = 400000, // Fast I2C speed
    };

    if (i2c_master_bus_add_device(bus_handle, &dev_cfg, &handle->i2c_dev) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add CH422G device to I2C bus");
        free(handle);
        return NULL;
    }

    handle->dir_cache = 0x0000; // Default all pins to input

    ESP_LOGI(TAG, "CH422G device initialized at address 0x%02X", dev_addr);
    return handle;
}

esp_err_t ch422g_deinit(ch422g_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    i2c_master_bus_rm_device(handle->i2c_dev);
    free(handle);
    return ESP_OK;
}

esp_err_t ch422g_pin_mode(ch422g_handle_t handle, uint8_t pin_num, uint8_t mode)
{
    if (handle == NULL || pin_num > 15) {
        return ESP_ERR_INVALID_ARG;
    }

    if (mode == CH422G_OUTPUT) {
        handle->dir_cache |= (1 << pin_num);
    } else {
        handle->dir_cache &= ~(1 << pin_num);
    }

    uint8_t port_a_dir = handle->dir_cache & 0xFF;
    uint8_t port_b_dir = (handle->dir_cache >> 8) & 0xFF;

    uint8_t cmd_buf[3] = {CMD_SET_IO_DIR, port_a_dir, port_b_dir};

    esp_err_t ret = i2c_master_transmit(handle->i2c_dev, cmd_buf, sizeof(cmd_buf), pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set pin mode: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t ch422g_digital_write(ch422g_handle_t handle, uint8_t pin_num, uint8_t level)
{
    if (handle == NULL || pin_num > 15) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t i2c_cmd = level ? CMD_SET_BIT_OUT : CMD_CLR_BIT_OUT;
    uint8_t port_a_mask = (pin_num < 8) ? (1 << pin_num) : 0;
    uint8_t port_b_mask = (pin_num >= 8) ? (1 << (pin_num - 8)) : 0;

    uint8_t cmd_buf[3] = {i2c_cmd, port_a_mask, port_b_mask};

    esp_err_t ret = i2c_master_transmit(handle->i2c_dev, cmd_buf, sizeof(cmd_buf), pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write to pin %d: %s", pin_num, esp_err_to_name(ret));
    }

    return ret;
}