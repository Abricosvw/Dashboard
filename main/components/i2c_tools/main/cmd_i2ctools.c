/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* cmd_i2ctools.c - Simplified I2C tools without console dependency

   This code provides basic I2C functionality without requiring esp_console.
*/
#include <stdio.h>
#include <string.h>
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "cmd_i2ctools"; // Tag for logging

#define I2C_TOOL_TIMEOUT_VALUE_MS (50) // Timeout value for I2C operations
static uint32_t i2c_frequency = 100 * 1000; // Default I2C frequency
i2c_master_bus_handle_t tool_bus_handle; // Handle for I2C master bus

// Simplified I2C tools initialization without console commands
void register_i2ctools(void)
{
    ESP_LOGI(TAG, "I2C tools initialized (console commands disabled)");
    ESP_LOGI(TAG, "I2C bus handle available for direct usage");
}

// Helper function for I2C device detection
esp_err_t i2c_detect_device(uint8_t device_addr)
{
    if (tool_bus_handle == NULL) {
        ESP_LOGE(TAG, "I2C bus not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Create device configuration
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = device_addr,
        .scl_speed_hz = i2c_frequency,
    };
    
    i2c_master_dev_handle_t dev_handle;
    esp_err_t ret = i2c_master_bus_add_device(tool_bus_handle, &dev_cfg, &dev_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Try to probe the device
    ret = i2c_master_probe(tool_bus_handle, device_addr, I2C_TOOL_TIMEOUT_VALUE_MS);
    
    // Clean up
    i2c_master_bus_rm_device(dev_handle);
    
    return ret;
}

// Simple I2C scan function
void i2c_scan_devices(void)
{
    ESP_LOGI(TAG, "Scanning I2C bus...");
    
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        esp_err_t ret = i2c_detect_device(addr);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Found device at address 0x%02X", addr);
        }
    }
    
    ESP_LOGI(TAG, "I2C scan completed");
}