#ifndef I2C_UTILS_H
#define I2C_UTILS_H

#include "driver/i2c_master_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Registers I2C utility commands with the console.
 *
 * This function registers the following commands for debugging:
 * - i2cscan: Scans the I2C bus for connected devices.
 * - i2cget: Reads a value from a register of an I2C device.
 * - i2cset: Writes a value to a register of an I2C device.
 *
 * It should be called after the I2C bus and the console are initialized.
 * @param bus_handle Handle to the initialized I2C master bus.
 */
void i2c_utils_register_commands(i2c_master_bus_handle_t bus_handle);

#ifdef __cplusplus
}
#endif

#endif // I2C_UTILS_H