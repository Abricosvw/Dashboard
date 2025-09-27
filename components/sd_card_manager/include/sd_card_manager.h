#ifndef SD_CARD_MANAGER_H
#define SD_CARD_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the SD card using the CH422G I/O expander for CS.
 *
 * This function must be called after the I2C bus and the CH422G expander are initialized.
 * It configures the SPI bus and mounts the FAT filesystem at "/sdcard".
 * It implements a wrapper around low-level SPI transactions to control the CS pin.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t sd_card_init(void);

/**
 * @brief Deinitializes the SD card, unmounts the filesystem, and frees resources.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t sd_card_deinit(void);

/**
 * @brief Writes data to a file on the SD card, overwriting the file if it exists.
 *
 * This function is thread-safe.
 *
 * @param path Full path to the file (e.g., "/sdcard/myfile.txt").
 * @param data The string data to write to the file.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t sd_card_write_file(const char* path, const char* data);

/**
 * @brief Appends data to a file on the SD card. Creates the file if it doesn't exist.
 *
 * This function is thread-safe.
 *
 * @param path Full path to the file (e.g., "/sdcard/log.txt").
 * @param data The string data to append to the file.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t sd_card_append_file(const char* path, const char* data);

/**
 * @brief Checks if the SD card is initialized and mounted.
 *
 * @return true if the SD card is ready for file operations, false otherwise.
 */
bool sd_card_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif // SD_CARD_MANAGER_H