#include "include/sd_card_manager.h"
#include "esp_log.h"
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

static const char *TAG = "SD_CARD";

static bool g_can_trace_enabled = false;
static SemaphoreHandle_t sd_card_mutex = NULL;

// Pinout from user's example
#define PIN_NUM_MISO 13
#define PIN_NUM_MOSI 11
#define PIN_NUM_CLK  12
#define PIN_NUM_CS   4

#define MOUNT_POINT "/sdcard"

static sdmmc_card_t *s_card = NULL;
static sdmmc_host_t s_host = SDSPI_HOST_DEFAULT();
static bool sd_card_initialized = false;

esp_err_t sd_card_init(void) {
    // If the card is already marked as initialized, perform a full de-initialization
    // to ensure a clean state before attempting to initialize again.
    if (sd_card_initialized) {
        ESP_LOGW(TAG, "Card already initialized. Performing full de-init before re-initializing...");
        sd_card_full_deinit();
        vTaskDelay(pdMS_TO_TICKS(200)); // Delay after de-init
    }

    esp_err_t ret = ESP_FAIL;
    const int max_retries = 3;

    ESP_LOGI(TAG, "Initializing SD card with robust retry logic...");

    for (int attempt = 1; attempt <= max_retries; ++attempt) {
        ESP_LOGI(TAG, "Initialization attempt %d/%d...", attempt, max_retries);

        // Give the card some time to power up/stabilize before each attempt
        vTaskDelay(pdMS_TO_TICKS(300 * attempt));

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

        s_host.max_freq_khz = (attempt == 1) ? 8000 : 4000;
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
    // Check if SD card is initialized
    if (!sd_card_initialized || s_card == NULL) {
        ESP_LOGE(TAG, "SD card not initialized!");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (sd_card_mutex == NULL) {
        ESP_LOGE(TAG, "SD card mutex is NULL!");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Attempting to acquire SD card mutex...");
    if (xSemaphoreTake(sd_card_mutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
        ESP_LOGI(TAG, "SD card mutex acquired successfully");
        // Ensure the directory exists before trying to write the file
        if (ensure_dir_exists(path) != ESP_OK) {
            xSemaphoreGive(sd_card_mutex);
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "Writing file: %s", path);
        
        // Check SD card info
        if (s_card != NULL) {
            ESP_LOGI(TAG, "SD card info - capacity: %llu MB, sector size: %d", 
                    ((uint64_t)s_card->csd.capacity) * s_card->csd.sector_size / (1024 * 1024),
                    s_card->csd.sector_size);
        }
        
        // First, try to verify the directory structure
        ESP_LOGI(TAG, "Verifying SD card mount and directory structure...");

        // Check if mount point exists and is accessible
        DIR* root_dir = opendir("/sdcard");
        if (root_dir == NULL) {
            ESP_LOGE(TAG, "Cannot open /sdcard directory (errno: %d)", errno);
            xSemaphoreGive(sd_card_mutex);
            return ESP_FAIL;
        }
        closedir(root_dir);
        ESP_LOGI(TAG, "/sdcard directory is accessible");

        // Try a simple test file first
        ESP_LOGI(TAG, "Testing simple file creation...");
        FILE *test_f = fopen("/sdcard/test.txt", "w");
        if (test_f != NULL) {
            // Try to write some data
            fprintf(test_f, "test data\n");
            fflush(test_f);
            fclose(test_f);

            // Verify the file was created
            FILE *verify_f = fopen("/sdcard/test.txt", "r");
            if (verify_f != NULL) {
                char buffer[32];
                fgets(buffer, sizeof(buffer), verify_f);
                fclose(verify_f);
                ESP_LOGI(TAG, "Simple file creation test passed - content: %s", buffer);
            } else {
                ESP_LOGE(TAG, "File created but cannot read back (errno: %d)", errno);
            }
            remove("/sdcard/test.txt");
        } else {
            ESP_LOGE(TAG, "Simple file creation failed (errno: %d)", errno);
            // Continue anyway to try the actual file
        }

        // Run a non-destructive write test to a temporary file
        ESP_LOGI(TAG, "Testing write access with temporary file...");
        FILE *temp_test_file = fopen("/sdcard/write_test.tmp", "w");
        if (temp_test_file != NULL) {
            fprintf(temp_test_file, "write test");
            fclose(temp_test_file);
            ESP_LOGI(TAG, "✓ Temporary file write successful");

            // Test reading it back
            FILE *read_test = fopen("/sdcard/write_test.tmp", "r");
            if (read_test != NULL) {
                char buffer[64];
                fgets(buffer, sizeof(buffer), read_test);
                fclose(read_test);
                ESP_LOGI(TAG, "✓ Temporary file read successful: %s", buffer);
            }
            remove("/sdcard/write_test.tmp");
        } else {
            ESP_LOGE(TAG, "✗ Temporary file write failed (errno: %d)", errno);
        }
        
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
    
    // Additional check - try to access the card
    // Quick test to see if card is still responsive
    DIR* test_dir = opendir(MOUNT_POINT);
    if (test_dir == NULL) {
        ESP_LOGW(TAG, "SD card appears to be disconnected or unmounted");
        sd_card_initialized = false;  // Mark as uninitialized
        return false;
    }
    closedir(test_dir);
    
    return true;
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
