// Settings Configuration Implementation
#include "settings_config.h"
#include <esp_log.h>
#include <nvs_flash.h>
#include "sd_card.h" // Replaced sd_card_manager.h
#include <stdio.h>
#include <nvs.h>
#include <string.h>
#include <stdlib.h>
#include "../background_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "SETTINGS_CONFIG";
#define NVS_NAMESPACE "settings"
static touch_settings_t current_settings;

// SD card access protection
static SemaphoreHandle_t sd_card_mutex = NULL;

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
    
    // First check if SD card is initialized
    // TODO: Re-implement sd_card_is_initialized() check
    /*
    if (!sd_card_is_initialized()) {
        ESP_LOGW(TAG, "SD card is not initialized! Attempting to reinitialize...");
        
        // Try to reinitialize SD card
        esp_err_t reinit_result = waveshare_sd_card_init();
        if (reinit_result != ESP_OK) {
            ESP_LOGE(TAG, "Failed to reinitialize SD card! Error: %s", esp_err_to_name(reinit_result));
            return;
        } else {
            ESP_LOGI(TAG, "SD card reinitialized successfully");
        }
    }
    */
    
    // Get SD card info for diagnostics
    // TODO: Re-implement sd_card_get_info()
    /*
    uint64_t total_bytes, free_bytes;
    if (sd_card_get_info(&total_bytes, &free_bytes) == ESP_OK) {
        ESP_LOGI(TAG, "SD card space - Total: %llu MB, Free: %llu MB", 
                 total_bytes / (1024 * 1024), free_bytes / (1024 * 1024));
        
        // Temporarily disable free space check to test if write actually works
        // Check if there's enough space (at least 1KB free)
        if (free_bytes < 1024) {
            ESP_LOGW(TAG, "SD card reports low free space (%llu bytes), but trying to save anyway...", free_bytes);
            // Don't return here - try to save anyway
        }
    }
    */
    
    // Save to SD Card as JSON
    char json_buffer[256];
    settings_to_json(settings_to_save, json_buffer, sizeof(json_buffer));
    
    ESP_LOGI(TAG, "Attempting to save settings to SD card...");
    
    // Take mutex before SD card access
    if (sd_card_mutex == NULL || xSemaphoreTake(sd_card_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take SD card mutex for writing, operation aborted");
        return;
    }
    
    // Save settings as .txt file with 8.3 filename format for maximum compatibility
    esp_err_t result = s_example_write_file("/sdcard/settings.cfg", json_buffer);
    
    // Release mutex after file operations
    xSemaphoreGive(sd_card_mutex);
    
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "Settings saved to SD card successfully.");
    } else {
        ESP_LOGE(TAG, "Failed to save settings to SD card. Error: %s (0x%x)", 
                 esp_err_to_name(result), result);
    }
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
    // Initialize SD card mutex if not already done
    if (sd_card_mutex == NULL) {
        sd_card_mutex = xSemaphoreCreateMutex();
        if (sd_card_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create SD card mutex!");
            settings_init_defaults(&current_settings);
            return ESP_ERR_NO_MEM;
        }
        ESP_LOGI(TAG, "SD card access mutex created successfully");
    }
    
    // First check if SD card is initialized
    // TODO: Re-implement sd_card_is_initialized() check
    /*
    if (!sd_card_is_initialized()) {
        ESP_LOGW(TAG, "SD card is not initialized. Using default settings.");
        settings_init_defaults(&current_settings);
        return ESP_ERR_INVALID_STATE;
    }
    */
    
    // Try to load from SD card with mutex protection
    ESP_LOGI(TAG, "Attempting to load settings from SD card...");
    
    // Take mutex before SD card access
    if (xSemaphoreTake(sd_card_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to take SD card mutex for reading, using defaults");
        settings_init_defaults(&current_settings);
        return ESP_ERR_TIMEOUT;
    }
    
    FILE* f = fopen("/sdcard/settings.cfg", "r");
    if (f != NULL) {
        char buffer[256] = {0};
        size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, f);
        fclose(f);
        
        // Release mutex after file operations
        xSemaphoreGive(sd_card_mutex);
        
        ESP_LOGI(TAG, "Read %d bytes from settings.cfg: %s", bytes_read, buffer);
        
        if (settings_from_json(buffer, &current_settings)) {
            ESP_LOGI(TAG, "Settings loaded from settings.cfg successfully.");
            ESP_LOGI(TAG, "Loaded settings: Demo=%s, Screen3=%s, Sensitivity=%d", 
                     current_settings.demo_mode_enabled ? "ON" : "OFF",
                     current_settings.screen3_enabled ? "ON" : "OFF", 
                     current_settings.touch_sensitivity_level);
            return ESP_OK;
        } else {
            ESP_LOGW(TAG, "Failed to parse settings.cfg, using defaults.");
            settings_init_defaults(&current_settings);
            return ESP_FAIL;
        }
    }

    // Release mutex if file couldn't be opened
    xSemaphoreGive(sd_card_mutex);

    // If file doesn't exist or can't be opened, use defaults and try to create the file.
    ESP_LOGI(TAG, "settings.cfg not found on SD card, initializing with defaults.");
    settings_init_defaults(&current_settings);

    // Attempt to save the new default settings to the SD card.
    // This is a blocking call, but it only happens once on the very first boot.
    ESP_LOGI(TAG, "Attempting to create default settings file...");
    settings_save(&current_settings);

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
