/*
 * ECU Data Management for ECU Dashboard
 * Handles ECU data storage, updates, and data stream logging
 */

#include "include/ecu_data.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

static const char *TAG = "ECU_DATA";

// Global ECU data
static ecu_data_t g_ecu_data = {0};
static SemaphoreHandle_t ecu_data_mutex = NULL;

// System settings
static system_settings_t g_system_settings = {
    .max_boost_limit = 250.0f,
    .max_rpm_limit = 7000.0f,
    .audio_alerts_enabled = true,
    .ecu_address = "192.168.4.1"
};

// Data stream (simple circular buffer)
#define DATA_STREAM_SIZE 50
static data_stream_entry_t data_stream[DATA_STREAM_SIZE] = {0};
static int data_stream_index = 0;
static bool data_stream_initialized = false;

// Initialize ECU data system
void ecu_data_init(void)
{
    if (ecu_data_mutex == NULL) {
        ecu_data_mutex = xSemaphoreCreateMutex();
    }

    // Initialize ECU data with default values
    memset(&g_ecu_data, 0, sizeof(ecu_data_t));
    g_ecu_data.timestamp = esp_timer_get_time() / 1000; // milliseconds

    // Initialize data stream
    memset(data_stream, 0, sizeof(data_stream));
    data_stream_initialized = true;

    ESP_LOGI(TAG, "ECU data system initialized");
}

// Update ECU data (thread-safe)
void ecu_data_update(ecu_data_t *data)
{
    if (!data || !ecu_data_mutex) return;

    if (xSemaphoreTake(ecu_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(&g_ecu_data, data, sizeof(ecu_data_t));
        g_ecu_data.timestamp = esp_timer_get_time() / 1000; // milliseconds

        xSemaphoreGive(ecu_data_mutex);
    }
}

// ✅ REMOVED: ecu_data_get() was unsafe (returned pointer without mutex protection)
// Use ecu_data_get_copy() instead for thread-safe access

// Get a thread-safe copy of the current ECU data
void ecu_data_get_copy(ecu_data_t *data_copy)
{
    if (!data_copy || !ecu_data_mutex) return;

    if (xSemaphoreTake(ecu_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(data_copy, &g_ecu_data, sizeof(ecu_data_t));
        xSemaphoreGive(ecu_data_mutex);
    }
}

// ✅ REMOVED: ecu_data_to_json() and ecu_data_from_json() were unused stubs
// If JSON serialization is needed in the future, consider using cJSON library

// Simulate ECU data for testing
void ecu_data_simulate(ecu_data_t *data)
{
    // TODO: Update this function to simulate the new data fields if needed for testing.
    if (!data) return;

    static float sim_time = 0;
    sim_time += 0.1f;

    // Simulate realistic ECU data
    data->engine_rpm = 800 + 200 * sin(sim_time * 0.5f) + 100 * sin(sim_time * 2.0f);
    data->map_kpa = 100 + 50 * sin(sim_time * 0.8f) + 20 * sin(sim_time * 1.5f);
    data->tps_position = 20 + 30 * sin(sim_time * 0.3f) + 10 * sin(sim_time * 1.2f);

    data->timestamp = esp_timer_get_time() / 1000;
}

// ============================================================================
// SYSTEM SETTINGS FUNCTIONS
// ============================================================================

void system_settings_init(void)
{
    // Initialize with defaults (already done at declaration)
    ESP_LOGI(TAG, "System settings initialized");
}

system_settings_t* system_settings_get(void)
{
    return &g_system_settings;
}

void system_settings_save(const system_settings_t *settings)
{
    if (settings) {
        memcpy(&g_system_settings, settings, sizeof(system_settings_t));
        ESP_LOGI(TAG, "System settings saved");
    }
}

// ============================================================================
// DATA STREAM FUNCTIONS
// ============================================================================

void data_stream_add_entry(const char *message, log_type_t type)
{
    if (!message || !data_stream_initialized) return;

    // Add new entry
    data_stream[data_stream_index].timestamp = esp_timer_get_time() / 1000;
    data_stream[data_stream_index].type = type;
    strncpy(data_stream[data_stream_index].message, message,
            sizeof(data_stream[data_stream_index].message) - 1);
    data_stream[data_stream_index].message[sizeof(data_stream[data_stream_index].message) - 1] = '\0';

    // Move to next index (circular buffer)
    data_stream_index = (data_stream_index + 1) % DATA_STREAM_SIZE;
}

void data_stream_clear(void)
{
    memset(data_stream, 0, sizeof(data_stream));
    data_stream_index = 0;
}

char* data_stream_to_json(void)
{
    static char json_buffer[4096];
    char *ptr = json_buffer;

    ptr += sprintf(ptr, "[");

    for (int i = 0; i < DATA_STREAM_SIZE; i++) {
        int index = (data_stream_index - 1 - i + DATA_STREAM_SIZE) % DATA_STREAM_SIZE;

        if (data_stream[index].timestamp == 0) continue; // Skip empty entries

        if (ptr > json_buffer + 1) {
            ptr += sprintf(ptr, ",");
        }

        const char *type_str;
        switch (data_stream[index].type) {
            case LOG_INFO: type_str = "info"; break;
            case LOG_WARNING: type_str = "warning"; break;
            case LOG_SUCCESS: type_str = "success"; break;
            case LOG_ERROR: type_str = "error"; break;
            default: type_str = "info"; break;
        }

        ptr += sprintf(ptr, "{\"timestamp\":%llu,\"message\":\"%s\",\"type\":\"%s\"}",
                       data_stream[index].timestamp,
                       data_stream[index].message,
                       type_str);
    }

    ptr += sprintf(ptr, "]");
    return json_buffer;
}

// ============================================================================
// DATA STREAM STRING FUNCTIONS
// ============================================================================
// ✅ REMOVED: ecu_data_to_string() was unused stub that always returned "No data"

char* data_stream_to_string(void)
{
    static char buffer[1024];
    char *ptr = buffer;

    for (int i = 0; i < DATA_STREAM_SIZE && ptr < buffer + sizeof(buffer) - 100; i++) {
        int index = (data_stream_index - 1 - i + DATA_STREAM_SIZE) % DATA_STREAM_SIZE;

        if (data_stream[index].timestamp == 0) continue;

        ptr += sprintf(ptr, "[%llu] %s\n",
                       data_stream[index].timestamp,
                       data_stream[index].message);
    }

    return buffer;
}