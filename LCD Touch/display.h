#ifndef DISPLAY_H
#define DISPLAY_H

#include "lvgl.h"
#include "driver/i2c_master_bus.h" // Modern I2C bus API

#ifdef __cplusplus
extern "C" {
#endif

// --- I2C Configuration for Touch Panel ---
// These definitions are used by main.c to initialize the I2C bus
#define I2C_HOST_NUM              I2C_NUM_0 /*!< I2C port number */
#define I2C_HOST_SCL_IO           9         /*!< GPIO number for I2C master clock */
#define I2C_HOST_SDA_IO           8         /*!< GPIO number for I2C master data  */
#define I2C_HOST_FREQ_HZ          400000    /*!< I2C master clock frequency */

/**
 * @brief Initializes the LCD panel, LVGL library, and touch controller.
 *
 * This function must be called AFTER the I2C bus has been initialized.
 * It sets up the RGB LCD panel, LVGL, and the GT911 touch controller.
 *
 * @param bus_handle The handle to the initialized I2C master bus.
 */
void display_init(i2c_master_bus_handle_t bus_handle);

/**
 * @brief Locks the LVGL mutex to ensure thread-safe access to LVGL objects.
 *
 * @param timeout_ms The maximum time to wait for the mutex. Use -1 to wait indefinitely.
 * @return `true` if the mutex was acquired, `false` otherwise.
 */
bool example_lvgl_lock(int timeout_ms);

/**
 * @brief Unlocks the LVGL mutex.
 */
void example_lvgl_unlock(void);

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_H