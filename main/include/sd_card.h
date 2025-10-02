#ifndef _SD_CARD_
#define _SD_CARD_

#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
// Removed old I2C driver include - I2C is now handled by display driver

// Add declarations for file I/O functions
esp_err_t s_example_write_file(const char *path, char *data);
esp_err_t s_example_read_file(const char *path);

// CH422G SD_CS control functions
esp_err_t sd_cs_set_high(void);  // Deselect SD card
esp_err_t sd_cs_set_low(void);   // Select SD card

#ifdef __cplusplus
extern "C" {
#endif
// I2C configuration constants removed - handled by display driver

// Maximum character size for file operations
#define EXAMPLE_MAX_CHAR_SIZE 64

// Mount point for the SD card
#define MOUNT_POINT "/sdcard"

// Pin assignments for SD SPI interface
#define PIN_NUM_MISO    13   /*!< Pin number for MISO */
#define PIN_NUM_MOSI    11   /*!< Pin number for MOSI */
#define PIN_NUM_CLK     12   /*!< Pin number for CLK */
#define PIN_NUM_CS      10   /*!< Pin number for CS */

// Function prototypes for initializing and testing SD card functions
esp_err_t waveshare_sd_card_init();
esp_err_t waveshare_sd_card_test();

#endif
