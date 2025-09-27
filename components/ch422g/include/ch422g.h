#ifndef CH422G_H
#define CH422G_H

#include "driver/i2c_master_bus.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle for the CH422G device
typedef struct ch422g_dev_t* ch422g_handle_t;

// --- CH422G I2C Addresses ---
#define CH422G_I2C_ADDRESS_0    0x24  // Default address
#define CH422G_I2C_ADDRESS_1    0x25
#define CH422G_I2C_ADDRESS_2    0x38
#define CH422G_I2C_ADDRESS_3    0x23

// --- Pin Modes ---
#define CH422G_OUTPUT   0x01
#define CH422G_INPUT    0x00

/**
 * @brief Initialize the CH422G device on a modern I2C bus.
 *
 * @param bus_handle Handle to the initialized I2C master bus.
 * @param dev_addr I2C address of the CH422G device.
 * @return Handle to the CH422G device, or NULL on failure.
 */
ch422g_handle_t ch422g_init(i2c_master_bus_handle_t bus_handle, uint8_t dev_addr);

/**
 * @brief Deinitialize the CH422G device and free resources.
 *
 * @param handle Handle to the CH422G device.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t ch422g_deinit(ch422g_handle_t handle);

/**
 * @brief Set the mode for a specific GPIO pin on the CH422G.
 *
 * @param handle Handle to the CH422G device.
 * @param pin_num The pin number to configure (0-7 for GPIOA, 8-15 for GPIOB).
 * @param mode The mode to set (e.g., CH422G_OUTPUT).
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t ch422g_pin_mode(ch422g_handle_t handle, uint8_t pin_num, uint8_t mode);

/**
 * @brief Write a digital value (HIGH or LOW) to a specific GPIO pin.
 *
 * @param handle Handle to the CH422G device.
 * @param pin_num The pin number to write to.
 * @param level The level to write (0 for LOW, 1 for HIGH).
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t ch422g_digital_write(ch422g_handle_t handle, uint8_t pin_num, uint8_t level);

#ifdef __cplusplus
}
#endif

#endif // CH422G_H