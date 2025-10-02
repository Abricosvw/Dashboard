#ifndef SD_CARD_MANAGER_H
#define SD_CARD_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the SD card and mounts the FAT filesystem.
 *
 * This function configures the SPI bus, initializes the SD card in SPI mode,
 * and mounts the filesystem at "/sdcard". This must be called once at startup.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t sd_card_init(void);

/**
 * @brief Deinitializes the SD card and unmounts the filesystem.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t sd_card_deinit(void);

/**
 * @brief Fully deinitializes the SD card and frees the SPI bus.
 *
 * Use this only when completely shutting down SD card functionality.
 * For reinitialization, use sd_card_deinit() instead.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t sd_card_full_deinit(void);

/**
 * @brief Performs a stability test on the SD card connection.
 *
 * This function runs multiple read/write operations to test the reliability
 * of the SD card connection and reports the success rate.
 *
 * @return ESP_OK if stable (>=80% success), ESP_ERR_INVALID_RESPONSE if unstable but usable (>=50%), ESP_FAIL if very unstable
 */
esp_err_t sd_card_stability_test(void);

/**
 * @brief Writes data to a file on the SD card, overwriting the file if it exists.
 *
 * @param path Full path to the file (e.g., "/sdcard/myfile.txt").
 * @param data The string data to write to the file.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t sd_card_write_file(const char* path, const char* data);

/**
 * @brief Appends data to a file on the SD card. Creates the file if it doesn't exist.
 *
 * @param path Full path to the file (e.g., "/sdcard/log.txt").
 * @param data The string data to append to the file.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t sd_card_append_file(const char* path, const char* data);

/**
 * @brief Enables or disables the logging of CAN bus traces to the SD card.
 *
 * @param enabled true to enable, false to disable.
 */
void sd_card_set_can_trace_enabled(bool enabled);

/**
 * @brief Checks if CAN bus trace logging is currently enabled.
 *
 * @return true if enabled, false otherwise.
 */
bool sd_card_is_can_trace_enabled(void);

/**
 * @brief Checks if SD card is initialized and ready for use.
 *
 * @return true if SD card is initialized and ready, false otherwise.
 */
bool sd_card_is_initialized(void);

/**
 * @brief Gets information about SD card status.
 *
 * @param total_bytes Pointer to store total capacity in bytes (can be NULL)
 * @param free_bytes Pointer to store free space in bytes (can be NULL)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sd_card_get_info(uint64_t *total_bytes, uint64_t *free_bytes);

/**
 * @brief Runs a comprehensive diagnostic test on the SD card.
 *
 * This function performs various tests including initialization check,
 * card info retrieval, filesystem info, read/write tests, and directory listing.
 * Results are printed to the console log.
 */
void sd_card_diagnostic_test(void);


#ifdef __cplusplus
}
#endif

#endif // SD_CARD_MANAGER_H
