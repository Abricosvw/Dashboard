#include "include/sd_card_manager.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "ch422g.h" // Our new C-driver for the expander

#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "SD_CARD_MGR";

// --- Hardware Configuration ---
#define MOUNT_POINT "/sdcard"
#define SPI_HOST_SLOT SPI2_HOST

#define PIN_NUM_MISO 13
#define PIN_NUM_MOSI 11
#define PIN_NUM_CLK  12
#define CH422G_SD_CS_IO 4 // I/O pin on the CH422G for SD Card CS

// --- Global handle from main.c ---
extern ch422g_handle_t g_ch422g_handle;

// --- Module State ---
static sdmmc_card_t *s_card = NULL;
static bool s_is_initialized = false;
static SemaphoreHandle_t s_mutex = NULL;
// Store the original transaction function to be restored on deinit
static esp_err_t (*s_orig_do_transaction)(int, sdmmc_command_t*);
// Store the host configuration to restore the function pointer
static sdmmc_host_t* s_host_ptr = NULL;


/**
 * @brief A wrapper around the original sdspi_host_do_transaction function.
 * This function handles Chip Select (CS) control via the CH422G I/O expander
 * before and after each low-level SPI transaction to the SD card.
 */
static esp_err_t ch422g_do_transaction(int slot, sdmmc_command_t* cmdinfo)
{
    if (g_ch422g_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_orig_do_transaction == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    // Assert CS (set LOW) before the transaction
    ch422g_digital_write(g_ch422g_handle, CH422G_SD_CS_IO, 0);

    // Call the original, real transaction function
    esp_err_t ret = s_orig_do_transaction(slot, cmdinfo);

    // De-assert CS (set HIGH) after the transaction
    ch422g_digital_write(g_ch422g_handle, CH422G_SD_CS_IO, 1);

    return ret;
}

esp_err_t sd_card_init(void)
{
    ESP_LOGI(TAG, "Initializing SD card with transaction-level CS control...");

    if (s_is_initialized) {
        ESP_LOGW(TAG, "SD card already initialized.");
        return ESP_OK;
    }

    if (g_ch422g_handle == NULL) {
        ESP_LOGE(TAG, "CH422G expander is not initialized! Cannot continue.");
        return ESP_ERR_INVALID_STATE;
    }

    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    ch422g_pin_mode(g_ch422g_handle, CH422G_SD_CS_IO, CH422G_OUTPUT);
    ch422g_digital_write(g_ch422g_handle, CH422G_SD_CS_IO, 1);
    vTaskDelay(pdMS_TO_TICKS(10));

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    esp_err_t ret = spi_bus_initialize(SPI_HOST_SLOT, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = GPIO_NUM_NC;
    slot_config.host_id = SPI_HOST_SLOT;
    
    // Allocate memory for the host configuration
    s_host_ptr = (sdmmc_host_t*)malloc(sizeof(sdmmc_host_t));
    if (s_host_ptr == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for host config");
        return ESP_ERR_NO_MEM;
    }
    *s_host_ptr = SDSPI_HOST_DEFAULT();
    s_host_ptr->slot = SPI_HOST_SLOT;

    // --- CRITICAL: Save the original function pointer and hook our wrapper ---
    s_orig_do_transaction = s_host_ptr->do_transaction;
    if (s_orig_do_transaction == NULL) {
        ESP_LOGE(TAG, "FATAL: s_host_ptr->do_transaction is NULL. Cannot hook transaction function.");
        free(s_host_ptr);
        s_host_ptr = NULL;
        return ESP_FAIL;
    }
    s_host_ptr->do_transaction = &ch422g_do_transaction;
    // ---

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    ESP_LOGI(TAG, "Mounting filesystem...");
    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, s_host_ptr, &slot_config, &mount_config, &s_card);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount filesystem: %s", esp_err_to_name(ret));
        s_host_ptr->do_transaction = s_orig_do_transaction; // Restore before freeing
        free(s_host_ptr);
        s_host_ptr = NULL;
        spi_bus_free(SPI_HOST_SLOT);
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "Filesystem mounted successfully.");
    sdmmc_card_print_info(stdout, s_card);
    s_is_initialized = true;
    return ESP_OK;
}

esp_err_t sd_card_deinit(void)
{
    if (!s_is_initialized) {
        return ESP_OK;
    }
    ESP_LOGI(TAG, "Deinitializing SD card...");
    esp_vfs_fat_sdcard_unmount(MOUNT_POINT, s_card);
    s_card = NULL;

    if (s_host_ptr != NULL) {
        s_host_ptr->do_transaction = s_orig_do_transaction;
        free(s_host_ptr);
        s_host_ptr = NULL;
    }
    s_orig_do_transaction = NULL;

    spi_bus_free(SPI_HOST_SLOT);
    if (s_mutex) {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
    }
    s_is_initialized = false;
    ESP_LOGI(TAG, "SD card deinitialized.");
    return ESP_OK;
}

static esp_err_t ensure_dir_exists(const char *path) { /* ... same as before ... */ }
esp_err_t sd_card_write_file(const char* path, const char* data) { /* ... same as before ... */ }
esp_err_t sd_card_append_file(const char* path, const char* data) { /* ... same as before ... */ }
bool sd_card_is_initialized(void) { return s_is_initialized; }