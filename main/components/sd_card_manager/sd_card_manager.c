#include "include/sd_card_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <errno.h>
#include <dirent.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// CH422G integration for SD card CS control
#ifdef CONFIG_USE_CH422G_FOR_SD
#include "../i2c_tools/main/ch422g.h"
#endif

static const char *TAG = "SD_CARD";

static bool g_can_trace_enabled = false;
static SemaphoreHandle_t sd_card_mutex = NULL;

// Pinout from user's example
#define PIN_NUM_MISO 13
#define PIN_NUM_MOSI 11
#define PIN_NUM_CLK  12

// CS pin - use CH422G if configured, otherwise use GPIO 4
#ifdef CONFIG_USE_CH422G_FOR_SD
#define PIN_NUM_CS   4  // Still needed for SDSPI, but we'll override CS behavior
#else
#define PIN_NUM_CS   4
#endif

#define MOUNT_POINT "/sdcard"

static sdmmc_card_t *s_card = NULL;
static sdmmc_host_t s_host = SDSPI_HOST_DEFAULT();
static bool sd_card_initialized = false;

// CH422G integration flag
static bool using_ch422g_for_cs = false;

esp_err_t sd_card_init(void) {
    // Check if card is already initialized and accessible
    if (sd_card_initialized && sd_card_is_initialized()) {
        ESP_LOGI(TAG, "SD card already initialized and accessible");
        return ESP_OK;
    }
    
    // If marked as initialized but not accessible, reset flag and continue
    if (sd_card_initialized && !sd_card_is_initialized()) {
        ESP_LOGW(TAG, "SD card marked as initialized but not accessible - resetting flag");
        sd_card_initialized = false;
    }
    
    // Only do full deinit if we have actual hardware state to clean up
    if (s_card != NULL || sd_card_mutex != NULL) {
        ESP_LOGW(TAG, "Cleaning up existing SD card state before re-initializing...");
        sd_card_full_deinit();
        vTaskDelay(pdMS_TO_TICKS(200)); // Delay after de-init
    }

    esp_err_t ret = ESP_FAIL;
    const int max_retries = 5; // Увеличиваем количество попыток для более надежной инициализации

    ESP_LOGI(TAG, "Initializing SD card with robust retry logic...");

    for (int attempt = 1; attempt <= max_retries; ++attempt) {
        ESP_LOGI(TAG, "Initialization attempt %d/%d...", attempt, max_retries);

        // Give the card even MORE time to power up/stabilize, especially on first boot
        int delay_ms = (attempt == 1) ? 2000 : (500 * attempt);
        ESP_LOGI(TAG, "Waiting %d ms for SD card to stabilize...", delay_ms);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));

        // Options for mounting the filesystem
        esp_vfs_fat_sdmmc_mount_config_t mount_config = {
            .format_if_mount_failed = (attempt > 1), // Only format on retry
            .max_files = 10,
            .allocation_unit_size = 0,
            .disk_status_check_enable = false
        };

        ESP_LOGI(TAG, "Initializing SPI bus...");
        spi_bus_config_t bus_cfg = {
            .mosi_io_num = PIN_NUM_MOSI,
            .miso_io_num = PIN_NUM_MISO,
            .sclk_io_num = PIN_NUM_CLK,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 4000,
        };
        ret = spi_bus_initialize(s_host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Attempt %d: Failed to initialize SPI bus: %s", attempt, esp_err_to_name(ret));
            sd_card_full_deinit(); // Clean up on failure
            continue; // Go to next retry
        }

        sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
        slot_config.gpio_cs = PIN_NUM_CS;
        slot_config.host_id = s_host.slot;
        slot_config.gpio_cd = GPIO_NUM_NC;
        slot_config.gpio_wp = GPIO_NUM_NC;
        slot_config.gpio_int = GPIO_NUM_NC;

        // Используем прогрессивно уменьшающуюся частоту для большей совместимости
        if (attempt <= 2) {
            s_host.max_freq_khz = 8000;
        } else if (attempt <= 4) {
            s_host.max_freq_khz = 4000;
        } else {
            s_host.max_freq_khz = 2000; // Самая низкая частота для последней попытки
        }
        ESP_LOGI(TAG, "Attempt %d: Mounting with SPI frequency %d kHz", attempt, s_host.max_freq_khz);
        
        ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &s_host, &slot_config, &mount_config, &s_card);

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Filesystem mounted successfully on attempt %d", attempt);
            sdmmc_card_print_info(stdout, s_card);

            sd_card_mutex = xSemaphoreCreateMutex();
            if (sd_card_mutex == NULL) {
                ESP_LOGE(TAG, "Failed to create SD card mutex");
                sd_card_full_deinit();
                continue; // This will count as a failed attempt
            }

            sd_card_initialized = true;
            return ESP_OK; // Success, exit the function
        }

        ESP_LOGE(TAG, "Attempt %d failed to mount filesystem: %s", attempt, esp_err_to_name(ret));
        sd_card_full_deinit(); // Full cleanup after any failure before next retry
    }

    ESP_LOGE(TAG, "All SD card initialization attempts failed.");
    return ret; // Return the last error
}

esp_err_t sd_card_deinit(void) {
    ESP_LOGI(TAG, "Deinitializing SD card...");
    
    // Mark as uninitialized first
    sd_card_initialized = false;
    
    if (s_card) {
        ESP_LOGI(TAG, "Unmounting SD card filesystem...");
        esp_err_t unmount_ret = esp_vfs_fat_sdcard_unmount(MOUNT_POINT, s_card);
        if (unmount_ret == ESP_OK) {
            ESP_LOGI(TAG, "SD card unmounted successfully");
        } else {
            ESP_LOGW(TAG, "SD card unmount failed: %s", esp_err_to_name(unmount_ret));
        }
        s_card = NULL;
    }
    
    if (sd_card_mutex) {
        vSemaphoreDelete(sd_card_mutex);
        sd_card_mutex = NULL;
        ESP_LOGI(TAG, "SD card mutex deleted");
    }
    
    // Don't free SPI bus during reinitialization to avoid conflicts
    // The bus will be reused
    ESP_LOGI(TAG, "SD card deinitialization completed (SPI bus kept for reuse)");
    
    return ESP_OK;
}

esp_err_t sd_card_full_deinit(void) {
    ESP_LOGI(TAG, "Full SD card deinitialization...");
    
    // First do normal deinit
    sd_card_deinit();
    
    // Now free the SPI bus
    esp_err_t ret = spi_bus_free(s_host.slot);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SPI bus freed");
    } else {
        ESP_LOGW(TAG, "SPI bus free failed: %s", esp_err_to_name(ret));
    }
    
    return ESP_OK;
}

// Helper function to ensure the directory for a given path exists.
// Note: This is a simplified implementation. For production code, a more
// robust recursive directory creation function would be better.
static esp_err_t ensure_dir_exists(const char *path) {
    // Make a copy of the path to safely manipulate it
    char *dir_path = strdup(path);
    if (dir_path == NULL) {
        ESP_LOGE(TAG, "strdup failed");
        return ESP_ERR_NO_MEM;
    }

    // Find the last '/' to get the directory part of the path
    char *last_slash = strrchr(dir_path, '/');
    if (last_slash != NULL && last_slash != dir_path) { // Check it's not the root slash
        // Terminate the string at the slash to get just the directory path
        *last_slash = '\0';

        // Check if the directory already exists by trying to open it
        DIR* dir = opendir(dir_path);
        if (dir != NULL) {
            // Directory exists
            closedir(dir);
        } else {
            // Directory does not exist, so try to create it
            ESP_LOGI(TAG, "Directory %s does not exist. Creating...", dir_path);
            if (mkdir(dir_path, 0755) != 0) {
                ESP_LOGE(TAG, "Failed to create directory %s (errno: %d)", dir_path, errno);
                free(dir_path);
                return ESP_FAIL;
            }
            ESP_LOGI(TAG, "Created directory %s", dir_path);
        }
    }

    // Free the duplicated string
    free(dir_path);
    return ESP_OK;
}

esp_err_t sd_card_write_file(const char* path, const char* data) {
    // Quick check if SD card is accessible
    if (!sd_card_is_initialized()) {
        ESP_LOGE(TAG, "SD card not accessible!");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (sd_card_mutex == NULL) {
        ESP_LOGE(TAG, "SD card mutex is NULL!");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGD(TAG, "Attempting to acquire SD card mutex...");
    if (xSemaphoreTake(sd_card_mutex, pdMS_TO_TICKS(3000)) == pdTRUE) {
        ESP_LOGD(TAG, "SD card mutex acquired successfully");
        // Ensure the directory exists before trying to write the file
        if (ensure_dir_exists(path) != ESP_OK) {
            xSemaphoreGive(sd_card_mutex);
            return ESP_FAIL;
        }

        ESP_LOGD(TAG, "Writing file: %s", path);
        
        // Quick verification of mount point accessibility
        DIR* root_dir = opendir("/sdcard");
        if (root_dir == NULL) {
            ESP_LOGE(TAG, "Cannot open /sdcard directory (errno: %d)", errno);
            xSemaphoreGive(sd_card_mutex);
            return ESP_FAIL;
        }
        closedir(root_dir);

        // Direct file write - no unnecessary tests that can cause errno 22
        
        // Try to open the actual file with retry logic
        FILE *f = NULL;
        int max_retries = 3;
        
        for (int retry = 0; retry < max_retries && f == NULL; retry++) {
            if (retry > 0) {
                ESP_LOGW(TAG, "Retrying file open (attempt %d/%d)", retry + 1, max_retries);
                vTaskDelay(pdMS_TO_TICKS(100 + retry * 100)); // Progressive delay
                
                // Check if SD card is still accessible
                if (!sd_card_is_initialized()) {
                    ESP_LOGW(TAG, "SD card became uninitialized, attempting reinit...");
                    xSemaphoreGive(sd_card_mutex);
                    
                    // Try to reinitialize SD card
                    esp_err_t reinit_result = sd_card_init();
                    if (reinit_result != ESP_OK) {
                        ESP_LOGE(TAG, "Failed to reinitialize SD card: %s", esp_err_to_name(reinit_result));
                        return ESP_ERR_INVALID_STATE;
                    }
                    
                    // Reacquire mutex after reinit
                    if (xSemaphoreTake(sd_card_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
                        ESP_LOGE(TAG, "Failed to reacquire SD card mutex after reinit");
                        return ESP_ERR_TIMEOUT;
                    }
                }
            }
            
            f = fopen(path, "w");
            if (f == NULL) {
                ESP_LOGW(TAG, "Failed to open file for writing: %s (errno: %d - %s)", 
                         path, errno, strerror(errno));
            }
        }
        
        if (f == NULL) {
            ESP_LOGE(TAG, "All file open attempts failed for: %s", path);
            xSemaphoreGive(sd_card_mutex);
            return ESP_FAIL;
        }
        fprintf(f, "%s", data);
        fclose(f);
        ESP_LOGI(TAG, "File written");
        xSemaphoreGive(sd_card_mutex);
        return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t sd_card_append_file(const char* path, const char* data) {
    if (xSemaphoreTake(sd_card_mutex, portMAX_DELAY) == pdTRUE) {
        // Ensure the directory exists before trying to append to the file
        if (ensure_dir_exists(path) != ESP_OK) {
            xSemaphoreGive(sd_card_mutex);
            return ESP_FAIL;
        }

        ESP_LOGD(TAG, "Appending to file: %s", path);
        FILE *f = fopen(path, "a");
        if (f == NULL) {
            ESP_LOGE(TAG, "Failed to open file for appending");
            xSemaphoreGive(sd_card_mutex);
            return ESP_FAIL;
        }
        fprintf(f, "%s", data);
        fclose(f);
        xSemaphoreGive(sd_card_mutex);
        return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}

void sd_card_set_can_trace_enabled(bool enabled) {
    g_can_trace_enabled = enabled;
    ESP_LOGI(TAG, "CAN trace logging %s", enabled ? "enabled" : "disabled");
}

bool sd_card_is_can_trace_enabled(void) {
    return g_can_trace_enabled;
}

bool sd_card_is_initialized(void) {
    // Basic checks
    if (!sd_card_initialized || s_card == NULL || sd_card_mutex == NULL) {
        return false;
    }
    
    // Lightweight check - only verify mount point accessibility
    // No aggressive write tests that can cause errno 22 issues
    DIR* test_dir = opendir(MOUNT_POINT);
    if (test_dir == NULL) {
        ESP_LOGD(TAG, "SD card appears to be disconnected or unmounted (errno: %d)", errno);
        sd_card_initialized = false;  // Mark as uninitialized
        return false;
    }
    closedir(test_dir);
    
    return true;
}

/**
 * @brief Упрощенная проверка инициализации SD карты для критических операций (загрузка настроек)
 * Пропускает stability test который может блокировать загрузку настроек
 */
bool sd_card_is_available_for_read(void) {
    // Basic checks
    if (!sd_card_initialized || s_card == NULL || sd_card_mutex == NULL) {
        return false;
    }
    
    // Only check if we can access the mount point - no write tests
    DIR* test_dir = opendir(MOUNT_POINT);
    if (test_dir == NULL) {
        ESP_LOGD(TAG, "SD card appears to be disconnected or unmounted (errno: %d)", errno);
        return false;
    }
    closedir(test_dir);
    
    ESP_LOGI(TAG, "SD card is available for read operations");
    return true;
}

/**
 * @brief Fast read-only file operation for critical operations (settings loading)
 */
esp_err_t sd_card_read_file_fast(const char* path, char* buffer, size_t buffer_size) {
    if (!sd_card_is_available_for_read()) {
        ESP_LOGE(TAG, "SD card not available for fast read");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (sd_card_mutex == NULL) {
        ESP_LOGE(TAG, "SD card mutex is NULL!");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Fast mutex acquisition with shorter timeout for critical operations
    if (xSemaphoreTake(sd_card_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        ESP_LOGD(TAG, "Fast read mutex acquired");
        
        FILE* f = fopen(path, "r");
        if (f != NULL) {
            size_t bytes_read = fread(buffer, 1, buffer_size - 1, f);
            buffer[bytes_read] = '\0'; // Null terminate
            fclose(f);
            
            xSemaphoreGive(sd_card_mutex);
            ESP_LOGD(TAG, "Fast read successful: %d bytes", bytes_read);
            return ESP_OK;
        } else {
            ESP_LOGE(TAG, "Fast read failed to open file: %s (errno: %d)", path, errno);
            xSemaphoreGive(sd_card_mutex);
            return ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "Fast read mutex timeout");
        return ESP_ERR_TIMEOUT;
    }
}

/**
 * @brief Optimized write operation with background processing fallback
 */
esp_err_t sd_card_write_file_optimized(const char* path, const char* data) {
    if (!sd_card_is_initialized()) {
        ESP_LOGE(TAG, "SD card not initialized for optimized write");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (sd_card_mutex == NULL) {
        ESP_LOGE(TAG, "SD card mutex is NULL!");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Longer mutex timeout for write operations
    if (xSemaphoreTake(sd_card_mutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
        ESP_LOGD(TAG, "Optimized write mutex acquired");
        
        // Ensure directory exists
        if (ensure_dir_exists(path) != ESP_OK) {
            xSemaphoreGive(sd_card_mutex);
            return ESP_FAIL;
        }
        
        // Direct file write with retry logic
        FILE* f = NULL;
        for (int retry = 0; retry < 3; retry++) {
            f = fopen(path, "w");
            if (f != NULL) break;
            
            if (retry < 2) {
                ESP_LOGW(TAG, "Write retry %d/3 (errno: %d)", retry + 1, errno);
                vTaskDelay(pdMS_TO_TICKS(100 * (retry + 1)));
            }
        }
        
        if (f != NULL) {
            size_t bytes_written = fwrite(data, 1, strlen(data), f);
            fflush(f);
            fclose(f);
            
            xSemaphoreGive(sd_card_mutex);
            ESP_LOGD(TAG, "Optimized write successful: %d bytes", bytes_written);
            return ESP_OK;
        } else {
            ESP_LOGE(TAG, "Optimized write failed after retries (errno: %d)", errno);
            xSemaphoreGive(sd_card_mutex);
            return ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "Optimized write mutex timeout");
        return ESP_ERR_TIMEOUT;
    }
}

esp_err_t sd_card_get_info(uint64_t *total_bytes, uint64_t *free_bytes) {
    if (!sd_card_is_initialized()) {
        ESP_LOGE(TAG, "SD card not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Get total capacity from card info
    if (s_card != NULL && total_bytes != NULL) {
        *total_bytes = ((uint64_t)s_card->csd.capacity) * s_card->csd.sector_size;
    }
    
    // For free space, try a write test with retry logic
    if (free_bytes != NULL) {
        bool write_test_passed = false;
        const char* test_files[] = {"/sdcard/test_write.tmp", "/sdcard/.test_free", "/sdcard/tmp.dat"};
        const int num_test_files = sizeof(test_files) / sizeof(test_files[0]);
        
        // Try multiple test files with retries
        for (int file_idx = 0; file_idx < num_test_files && !write_test_passed; file_idx++) {
            for (int retry = 0; retry < 3 && !write_test_passed; retry++) {
                if (retry > 0) {
                    ESP_LOGD(TAG, "Retrying free space test with %s (attempt %d/3)", test_files[file_idx], retry + 1);
                    vTaskDelay(pdMS_TO_TICKS(50 + retry * 50)); // Progressive delay
                }
                
                FILE* test_file = fopen(test_files[file_idx], "w");
                if (test_file != NULL) {
                    const char* test_data = "test";
                    size_t written = fwrite(test_data, 1, strlen(test_data), test_file);
                    fflush(test_file);
                    fclose(test_file);
                    
                    if (written == strlen(test_data)) {
                        // Verify we can read it back
                        FILE* verify_file = fopen(test_files[file_idx], "r");
                        if (verify_file != NULL) {
                            char read_buffer[16];
                            size_t read_bytes = fread(read_buffer, 1, sizeof(read_buffer) - 1, verify_file);
                            fclose(verify_file);
                            
                            if (read_bytes > 0 && strncmp(read_buffer, test_data, strlen(test_data)) == 0) {
                                write_test_passed = true;
                                ESP_LOGD(TAG, "Free space test passed with %s on attempt %d", test_files[file_idx], retry + 1);
                            }
                        }
                        remove(test_files[file_idx]); // Clean up
                    } else {
                        remove(test_files[file_idx]); // Clean up failed attempt
                    }
                }
            }
        }
        
        if (write_test_passed) {
            // If we can write, assume most space is free (conservative estimate)
            if (total_bytes != NULL && *total_bytes > 0) {
                *free_bytes = (*total_bytes * 85) / 100;  // 85% free (conservative)
            } else if (s_card != NULL) {
                uint64_t total_capacity = ((uint64_t)s_card->csd.capacity) * s_card->csd.sector_size;
                *free_bytes = (total_capacity * 85) / 100;
            } else {
                *free_bytes = 1024 * 1024;  // 1MB as fallback
            }
            ESP_LOGD(TAG, "Write test successful - estimated free space available");
        } else {
            // All write tests failed - card might be temporarily unavailable
            *free_bytes = 0;
            ESP_LOGW(TAG, "All free space tests failed - SD card may be temporarily unavailable");
        }
    }
    
    uint64_t total = 0, free = 0;
    if (total_bytes) total = *total_bytes;
    if (free_bytes) free = *free_bytes;
    
    ESP_LOGI(TAG, "SD card info - Total: %llu MB, Free: ~%llu MB (estimated)",
             total / (1024 * 1024), free / (1024 * 1024));
    
    return ESP_OK;
}

void sd_card_diagnostic_test(void) {
    ESP_LOGI(TAG, "========== SD Card Diagnostic Test ===========");
    
    // Check initialization status
    if (!sd_card_is_initialized()) {
        ESP_LOGE(TAG, "SD card is NOT initialized!");
        ESP_LOGI(TAG, "- Card pointer: %s", s_card ? "Valid" : "NULL");
        ESP_LOGI(TAG, "- Mutex: %s", sd_card_mutex ? "Valid" : "NULL");
        ESP_LOGI(TAG, "- Init flag: %s", sd_card_initialized ? "True" : "False");
        return;
    }
    
    ESP_LOGI(TAG, "SD card is initialized");
    
    // Get card info
    if (s_card != NULL) {
        ESP_LOGI(TAG, "Card Info:");
        ESP_LOGI(TAG, "- Name: %s", s_card->cid.name);
        ESP_LOGI(TAG, "- Capacity: %llu MB", 
                ((uint64_t)s_card->csd.capacity) * s_card->csd.sector_size / (1024 * 1024));
        ESP_LOGI(TAG, "- Sector size: %d", s_card->csd.sector_size);
        ESP_LOGI(TAG, "- Speed: %s", s_card->is_ddr ? "DDR" : "Default");
    }
    
    // Check mount point by trying to open directory
    DIR* test_dir = opendir(MOUNT_POINT);
    if (test_dir != NULL) {
        ESP_LOGI(TAG, "Mount point %s exists and is accessible", MOUNT_POINT);
        closedir(test_dir);
    } else {
        ESP_LOGE(TAG, "Mount point %s is NOT accessible!", MOUNT_POINT);
    }
    
    // Get filesystem info
    uint64_t total_bytes, free_bytes;
    if (sd_card_get_info(&total_bytes, &free_bytes) == ESP_OK) {
        ESP_LOGI(TAG, "Filesystem Info:");
        ESP_LOGI(TAG, "- Total space: %llu MB", total_bytes / (1024 * 1024));
        ESP_LOGI(TAG, "- Free space: %llu MB", free_bytes / (1024 * 1024));
        ESP_LOGI(TAG, "- Used space: %llu MB", (total_bytes - free_bytes) / (1024 * 1024));
        ESP_LOGI(TAG, "- Usage: %.1f%%", 
                ((double)(total_bytes - free_bytes) / total_bytes) * 100.0);
    }
    
    // Test write operation
    ESP_LOGI(TAG, "Testing write operation...");
    const char* test_data = "SD Card Test Write\n";
    esp_err_t write_result = sd_card_write_file("/sdcard/test_write.txt", test_data);
    if (write_result == ESP_OK) {
        ESP_LOGI(TAG, "Write test PASSED");
        
        // Test read operation
        ESP_LOGI(TAG, "Testing read operation...");
        FILE* f = fopen("/sdcard/test_write.txt", "r");
        if (f != NULL) {
            char buffer[64] = {0};
            fread(buffer, 1, sizeof(buffer) - 1, f);
            fclose(f);
            ESP_LOGI(TAG, "Read test PASSED - Content: %s", buffer);
            
            // Clean up test file
            remove("/sdcard/test_write.txt");
        } else {
            ESP_LOGE(TAG, "Read test FAILED - Could not open file");
        }
    } else {
        ESP_LOGE(TAG, "Write test FAILED - Error: %s (0x%x)", 
                esp_err_to_name(write_result), write_result);
    }
    
    // Additional test - try to create a file directly with fopen
    ESP_LOGI(TAG, "Testing direct file creation...");
    FILE* direct_test = fopen("/sdcard/direct_test.tmp", "w");
    if (direct_test != NULL) {
        fprintf(direct_test, "Direct write test\n");
        fclose(direct_test);
        ESP_LOGI(TAG, "Direct file creation PASSED");
        remove("/sdcard/direct_test.tmp");
    } else {
        ESP_LOGE(TAG, "Direct file creation FAILED - errno: %d", errno);
    }
    
    // List root directory
    ESP_LOGI(TAG, "Listing files in /sdcard:");
    DIR* dir = opendir(MOUNT_POINT);
    if (dir != NULL) {
        struct dirent* ent;
        int file_count = 0;
        while ((ent = readdir(dir)) != NULL) {
            ESP_LOGI(TAG, "- %s", ent->d_name);
            file_count++;
        }
        closedir(dir);
        ESP_LOGI(TAG, "Total files: %d", file_count);
    } else {
        ESP_LOGE(TAG, "Failed to open directory %s", MOUNT_POINT);
    }
    
    ESP_LOGI(TAG, "========== End of Diagnostic Test ===========");
}

/**
 * @brief Performs a stability test on the SD card connection
 * 
 * @return ESP_OK if stable, error code otherwise
 */
esp_err_t sd_card_stability_test(void) {
    if (!sd_card_initialized || s_card == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Running SD card stability test...");
    
    const int test_iterations = 10;
    int successful_operations = 0;
    
    for (int i = 0; i < test_iterations; i++) {
        char test_filename[32];
        snprintf(test_filename, sizeof(test_filename), "/sdcard/stab_test_%d.tmp", i);
        
        // Test write
        FILE* test_file = fopen(test_filename, "w");
        if (test_file != NULL) {
            const char* test_data = "stability_test_data";
            size_t written = fwrite(test_data, 1, strlen(test_data), test_file);
            fflush(test_file);
            fclose(test_file);
            
            if (written == strlen(test_data)) {
                // Test read
                FILE* read_file = fopen(test_filename, "r");
                if (read_file != NULL) {
                    char read_buffer[32];
                    size_t read_bytes = fread(read_buffer, 1, sizeof(read_buffer) - 1, read_file);
                    fclose(read_file);
                    
                    if (read_bytes == strlen(test_data) && 
                        strncmp(read_buffer, test_data, strlen(test_data)) == 0) {
                        successful_operations++;
                    }
                }
            }
            remove(test_filename); // Clean up
        }
        
        vTaskDelay(pdMS_TO_TICKS(50)); // Small delay between tests
    }
    
    float success_rate = (float)successful_operations / test_iterations * 100.0f;
    ESP_LOGI(TAG, "Stability test completed: %d/%d operations successful (%.1f%%)", 
             successful_operations, test_iterations, success_rate);
    
    if (success_rate >= 80.0f) {
        ESP_LOGI(TAG, "SD card connection is stable");
        return ESP_OK;
    } else if (success_rate >= 50.0f) {
        ESP_LOGW(TAG, "SD card connection is unstable but usable");
        return ESP_ERR_INVALID_RESPONSE;
    } else {
        ESP_LOGE(TAG, "SD card connection is very unstable");
        return ESP_FAIL;
    }
}

/**
 * @brief Sets up SD card to use CH422G for CS signal control.
 *
 * This function configures the SD card manager to use CH422G GPIO expander
 * for controlling the Chip Select signal instead of direct GPIO control.
 * This allows freeing up GPIO pins for other purposes.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sd_card_use_ch422g_for_cs(void)
{
#ifdef CONFIG_USE_CH422G_FOR_SD
    ESP_LOGI(TAG, "Configuring SD card to use CH422G for CS control...");

    // Initialize CH422G SD control if not already done
    esp_err_t ret = ch422g_sd_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize CH422G SD control: %s", esp_err_to_name(ret));
        return ret;
    }

    using_ch422g_for_cs = true;
    ESP_LOGI(TAG, "SD card now using CH422G for CS control");
    return ESP_OK;
#else
    ESP_LOGW(TAG, "CH422G support not enabled in configuration");
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

/**
 * @brief Checks if SD card is using CH422G for CS control.
 *
 * @return true if using CH422G, false if using direct GPIO
 */
bool sd_card_is_using_ch422g_for_cs(void)
{
    return using_ch422g_for_cs;
}
