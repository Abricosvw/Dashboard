/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register all i2ctools commands
 * 
 * @param bus_handle Handle of the I2C bus to be used by the commands.
 */
void register_i2ctools(i2c_master_bus_handle_t bus_handle);

extern i2c_master_bus_handle_t tool_bus_handle;

#ifdef __cplusplus
}
#endif
