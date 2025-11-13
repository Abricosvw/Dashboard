// Settings Configuration Implementation
#include "settings_config.h"
#include <esp_log.h>
#include <nvs_flash.h>
#include "sd_card_manager.h"
#include <stdio.h>
#include <nvs.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "../background_task.h"

static const char *TAG = "SETTINGS_CONFIG";
#define NVS_NAMESPACE "settings"
static touch_settings_t current_settings;

// Helper to serialize settings to a JSON string
static void settings_to_json(const touch_settings_t* settings, char* buffer, size_t buffer_size) {
    // A more robust implementation would use a proper JSON library
    snprintf(buffer, buffer_size,
             "{\"sensitivity\":%d,\"demo_mode\":%s,\"screen3_enabled\":%s}",
             settings->touch_sensitivity_level,
             settings->demo_mode_enabled ? "true" : "false",
             settings->screen3_enabled ? "true" : "false");
}

// Helper to deserialize settings from a JSON string
static bool settings_from_json(const char* json_str, touch_settings_t* settings) {
    // This is a very basic parser. A real implementation should use a JSON library like cJSON.
    const char* sens_key = "\"sensitivity\":";
    const char* demo_key = "\"demo_mode\":";
    const char* s3_key = "\"screen3_enabled\":";

    char* sens_ptr = strstr(json_str, sens_key);
    char* demo_ptr = strstr(json_str, demo_key);
    char* s3_ptr = strstr(json_str, s3_key);

    if (sens_ptr && demo_ptr && s3_ptr) {
        settings->touch_sensitivity_level = atoi(sens_ptr + strlen(sens_key));
        settings->demo_mode_enabled = strstr(demo_ptr, "true") != NULL;
        settings->screen3_enabled = strstr(s3_ptr, "true") != NULL;
        return true;
    }
    return false;
}

void settings_init_defaults(touch_settings_t *settings) {
    if (settings == NULL) return;
    settings->touch_sensitivity_level = DEFAULT_TOUCH_SENSITIVITY;
    settings->demo_mode_enabled = DEFAULT_DEMO_MODE_ENABLED;
    settings->screen3_enabled = DEFAULT_SCREEN3_ENABLED;
    for (int i = 0; i < SCREEN1_ARCS_COUNT; i++) settings->screen1_arcs_enabled[i] = true;
    for (int i = 0; i < SCREEN2_ARCS_COUNT; i++) settings->screen2_arcs_enabled[i] = true;
    
    ESP_LOGI(TAG, "Initialized default settings: Demo=%s, Screen3=%s, Sensitivity=%d", 
             settings->demo_mode_enabled ? "ON" : "OFF",
             settings->screen3_enabled ? "ON" : "OFF", 
             settings->touch_sensitivity_level);
}

/**
 * @brief Saves the provided settings struct to the SD card.
 * This is a slow, blocking function and should only be called from a background task
 * or during initial setup.
 * @param settings_to_save A pointer to the settings struct to save.
 */
void settings_save(const touch_settings_t *settings_to_save) {
    if (settings_to_save == NULL) {
        ESP_LOGE(TAG, "settings_save called with NULL data!");
        return;
    }
    
    // Validate settings before saving
    touch_settings_t temp_settings;
    memcpy(&temp_settings, settings_to_save, sizeof(touch_settings_t));
    if (!settings_validate(&temp_settings)) {
        ESP_LOGE(TAG, "Settings validation failed before save!");
        return;
    }
    
    ESP_LOGI(TAG, "Attempting to save settings to SD card...");
    ESP_LOGI(TAG, "Settings to save: Demo=%s, Screen3=%s, Sensitivity=%d", 
             settings_to_save->demo_mode_enabled ? "ON" : "OFF",
             settings_to_save->screen3_enabled ? "ON" : "OFF", 
             settings_to_save->touch_sensitivity_level);
    
    // Try multiple times with different approaches
    esp_err_t result = ESP_FAIL;
    
    for (int attempt = 1; attempt <= 3; attempt++) {
        ESP_LOGI(TAG, "Save attempt %d/3", attempt);
        
        // Quick check if SD card is accessible - optimized approach
        if (!sd_card_is_initialized()) {
            ESP_LOGW(TAG, "SD card is not accessible! Attempting to reinitialize...");
            
            // Try to reinitialize SD card with optimized approach
            esp_err_t reinit_result = sd_card_init();
            if (reinit_result != ESP_OK) {
                ESP_LOGE(TAG, "Failed to reinitialize SD card on attempt %d! Error: %s", 
                         attempt, esp_err_to_name(reinit_result));
                if (attempt < 3) {
                    vTaskDelay(pdMS_TO_TICKS(1000 * attempt)); // Longer progressive delay
                    continue;
                } else {
                    ESP_LOGW(TAG, "All reinitialization attempts failed - queuing for background processing");
                    trigger_settings_save(); // Queue for background processing
                    return;
                }
            } else {
                ESP_LOGI(TAG, "SD card reinitialized successfully");
                vTaskDelay(pdMS_TO_TICKS(100)); // Minimal stabilization delay
            }
        }
        
        // Get SD card info for diagnostics
        uint64_t total_bytes, free_bytes;
        if (sd_card_get_info(&total_bytes, &free_bytes) == ESP_OK) {
            ESP_LOGI(TAG, "SD card space - Total: %llu MB, Free: %llu MB", 
                     total_bytes / (1024 * 1024), free_bytes / (1024 * 1024));
        }
        
        // Create JSON with retry logic
        char json_buffer[512];
        memset(json_buffer, 0, sizeof(json_buffer));
        settings_to_json(settings_to_save, json_buffer, sizeof(json_buffer));
        
        // Verify JSON was created correctly
        if (strlen(json_buffer) == 0) {
            ESP_LOGE(TAG, "Failed to create JSON string for settings");
            continue;
        }
        
        ESP_LOGI(TAG, "JSON to save: %s", json_buffer);
        
        // Try to save using SD card manager
        result = sd_card_write_file("/sdcard/settings.cfg", json_buffer);
        
        if (result == ESP_OK) {
            ESP_LOGI(TAG, "Settings saved to SD card successfully on attempt %d", attempt);
            
            // Verify the file was saved correctly by reading it back
            vTaskDelay(pdMS_TO_TICKS(100)); // Small delay
            FILE* verify_file = fopen("/sdcard/settings.cfg", "r");
            if (verify_file != NULL) {
                char verify_buffer[512] = {0};
                size_t bytes_read = fread(verify_buffer, 1, sizeof(verify_buffer) - 1, verify_file);
                fclose(verify_file);
                
                if (bytes_read > 0 && strcmp(json_buffer, verify_buffer) == 0) {
                    ESP_LOGI(TAG, "Settings save verification successful");
                    return; // Success!
                } else {
                    ESP_LOGW(TAG, "Settings save verification failed on attempt %d", attempt);
                    result = ESP_FAIL;
                }
            } else {
                ESP_LOGW(TAG, "Cannot open saved file for verification on attempt %d", attempt);
                result = ESP_FAIL;
            }
        } else {
            ESP_LOGE(TAG, "Failed to save settings to SD card on attempt %d. Error: %s (0x%x)", 
                     attempt, esp_err_to_name(result), result);
        }
        
        if (attempt < 3) {
            ESP_LOGW(TAG, "Retrying settings save after delay...");
            vTaskDelay(pdMS_TO_TICKS(300 * attempt)); // Progressive delay
        }
    }
    
    ESP_LOGE(TAG, "All attempts to save settings failed!");
}

/**
 * @brief Queues a request to save the current settings in a background task.
 * This function makes a copy of the current settings to pass to the background task.
 */
void trigger_settings_save(void) {
    // Allocate memory for a copy of the settings to ensure thread safety.
    touch_settings_t *settings_copy = malloc(sizeof(touch_settings_t));
    if (settings_copy == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for settings copy!");
        return;
    }

    // Copy the current settings to the new memory block
    memcpy(settings_copy, &current_settings, sizeof(touch_settings_t));

    background_task_t task = {
        .type = BG_TASK_SETTINGS_SAVE,
        .data = settings_copy, // Pass the pointer to the copy
        .data_size = sizeof(touch_settings_t),
        .callback = NULL
    };

    if (background_task_add(&task) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to queue settings save task. Queue might be full.");
        free(settings_copy); // Free memory if task queuing fails
    } else {
        ESP_LOGI(TAG, "Settings save queued for background processing.");
    }
}

/**
 * @brief Loads settings from the SD card. If it fails, loads defaults.
 */
esp_err_t settings_load(void) {
    ESP_LOGI(TAG, "Starting settings load process...");
    
    // Initialize with defaults first as a fallback
    settings_init_defaults(&current_settings);
    
    // Check if SD card is available for reading (bypasses aggressive stability tests)
    if (!sd_card_is_available_for_read()) {
        ESP_LOGW(TAG, "SD card is not available for reading. Using default settings.");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Try to load from SD card with retry logic
    ESP_LOGI(TAG, "Attempting to load settings from SD card...");
    
    for (int retry = 0; retry < 3; retry++) {
        if (retry > 0) {
            ESP_LOGW(TAG, "Retrying settings load (attempt %d/3)", retry + 1);
            vTaskDelay(pdMS_TO_TICKS(100 * retry)); // Progressive delay
            
            // Check if SD card is still available
            if (!sd_card_is_initialized()) {
                ESP_LOGW(TAG, "SD card became unavailable during load retry");
                break;
            }
        }
        
        FILE* f = fopen("/sdcard/settings.cfg", "r");
        if (f != NULL) {
            char buffer[512] = {0}; // Увеличиваем буфер для надежности
            size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, f);
            fclose(f);
            
            if (bytes_read > 0) {
                ESP_LOGI(TAG, "Read %d bytes from settings.cfg: %s", bytes_read, buffer);
                
                // Пытаемся парсить настройки
                touch_settings_t temp_settings;
                if (settings_from_json(buffer, &temp_settings)) {
                    // Валидируем загруженные настройки
                    if (settings_validate(&temp_settings)) {
                        memcpy(&current_settings, &temp_settings, sizeof(touch_settings_t));
                        ESP_LOGI(TAG, "Settings loaded and validated successfully.");
                        ESP_LOGI(TAG, "Loaded settings: Demo=%s, Screen3=%s, Sensitivity=%d", 
                                 current_settings.demo_mode_enabled ? "ON" : "OFF",
                                 current_settings.screen3_enabled ? "ON" : "OFF", 
                                 current_settings.touch_sensitivity_level);
                        return ESP_OK;
                    } else {
                        ESP_LOGW(TAG, "Loaded settings failed validation, using defaults.");
                        settings_init_defaults(&current_settings);
                        return ESP_FAIL;
                    }
                } else {
                    ESP_LOGW(TAG, "Failed to parse settings.cfg on attempt %d", retry + 1);
                    if (retry == 2) { // Last attempt
                        ESP_LOGW(TAG, "All parsing attempts failed, using defaults.");
                        settings_init_defaults(&current_settings);
                        return ESP_FAIL;
                    }
                }
            } else {
                ESP_LOGW(TAG, "settings.cfg file is empty on attempt %d", retry + 1);
            }
        } else {
            if (retry == 0) {
                ESP_LOGI(TAG, "settings.cfg not found on SD card");
            }
            break; // File doesn't exist, no point in retrying
        }
    }

    // If we get here, either file doesn't exist or all load attempts failed
    ESP_LOGI(TAG, "Using default settings and attempting to create settings file");
    settings_init_defaults(&current_settings);

    // Try to save the default settings to create the file for next time
    // Do this in background to avoid blocking startup
    trigger_settings_save();

    return ESP_ERR_NOT_FOUND; // Return NOT_FOUND to indicate that defaults were loaded.
}

// ... other functions like settings_validate, getters/setters, etc. remain the same ...

bool settings_validate(touch_settings_t *settings) { if (settings == NULL) return false; if (settings->touch_sensitivity_level < MIN_TOUCH_SENSITIVITY || settings->touch_sensitivity_level > MAX_TOUCH_SENSITIVITY) return false; return true; }
void settings_print_debug(touch_settings_t *settings) { if(settings == NULL) return; ESP_LOGI(TAG, "Settings Debug: Touch=%d, Demo=%s, Screen3=%s", settings->touch_sensitivity_level, settings->demo_mode_enabled ? "ON":"OFF", settings->screen3_enabled ? "ON":"OFF"); }
bool demo_mode_get_enabled(void) { return current_settings.demo_mode_enabled; }
void demo_mode_set_enabled(bool enabled) { current_settings.demo_mode_enabled = enabled; }
bool screen3_get_enabled(void) { return current_settings.screen3_enabled; }
void screen3_set_enabled(bool enabled) { current_settings.screen3_enabled = enabled; }
void settings_apply_changes(void) { ESP_LOGI(TAG, "Applying settings changes..."); }
void settings_reset_to_defaults(void) { ESP_LOGI(TAG, "Resetting settings to defaults in memory"); settings_init_defaults(&current_settings); settings_apply_changes(); }
bool screen1_arc_get_enabled(int arc_index) { if (arc_index < 0 || arc_index >= SCREEN1_ARCS_COUNT) return false; return current_settings.screen1_arcs_enabled[arc_index]; }
void screen1_arc_set_enabled(int arc_index, bool enabled) { if (arc_index < 0 || arc_index >= SCREEN1_ARCS_COUNT) return; current_settings.screen1_arcs_enabled[arc_index] = enabled; }
bool screen2_arc_get_enabled(int arc_index) { if (arc_index < 0 || arc_index >= SCREEN2_ARCS_COUNT) return false; return current_settings.screen2_arcs_enabled[arc_index]; }
void screen2_arc_set_enabled(int arc_index, bool enabled) { if (arc_index < 0 || arc_index >= SCREEN2_ARCS_COUNT) return; current_settings.screen2_arcs_enabled[arc_index] = enabled; }
void ui_Screen1_update_arcs_visibility(void) { ESP_LOGD(TAG, "Screen1 arcs visibility update requested"); }
void ui_Screen2_update_arcs_visibility(void) { ESP_LOGD(TAG, "Screen2 arcs visibility update requested"); }
void demo_mode_test_toggle(void) { current_settings.demo_mode_enabled = !current_settings.demo_mode_enabled; }
void demo_mode_status_report(void) { ESP_LOGI(TAG, "Demo Mode Status: %s", current_settings.demo_mode_enabled ? "ENABLED" : "DISABLED"); }

/**
 * @brief Test function to verify settings persistence functionality
 */
void settings_test_persistence(void) {
    ESP_LOGI(TAG, "========== Settings Persistence Test ==========");
    
    // Show current settings
    ESP_LOGI(TAG, "Current settings: Demo=%s, Screen3=%s, Sensitivity=%d", 
             current_settings.demo_mode_enabled ? "ON" : "OFF",
             current_settings.screen3_enabled ? "ON" : "OFF", 
             current_settings.touch_sensitivity_level);
    
    // Save current settings
    ESP_LOGI(TAG, "Testing settings save...");
    settings_save(&current_settings);
    
    // Test load functionality
    ESP_LOGI(TAG, "Testing settings load...");
    esp_err_t load_result = settings_load();
    
    ESP_LOGI(TAG, "Load result: %s", esp_err_to_name(load_result));
    ESP_LOGI(TAG, "Settings after load: Demo=%s, Screen3=%s, Sensitivity=%d", 
             current_settings.demo_mode_enabled ? "ON" : "OFF",
             current_settings.screen3_enabled ? "ON" : "OFF", 
             current_settings.touch_sensitivity_level);
    
    ESP_LOGI(TAG, "========== End Settings Test ==========");
}
