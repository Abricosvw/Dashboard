#include "include/canbus.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>
#include "esp_timer.h"
#include "include/web_server.h"
#include "include/can_websocket.h"
#include "lvgl.h"
#include "ui/screens/ui_Screen3.h"
#include "include/can_parser.h"
#include "sd_card.h" // Replaced sd_card_manager.h

static const char *CAN_TAG = "CANBUS";

// CAN bus configuration
// The speed is set to 500kbit/s as per the user's new specification.
static twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_LISTEN_ONLY);
static twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
static twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

static bool canbus_initialized = false;
static bool canbus_running = false;

esp_err_t canbus_init(void)
{
    if (canbus_initialized) {
        ESP_LOGW(CAN_TAG, "CAN bus already initialized");
        return ESP_OK;
    }
    
    esp_err_t ret = twai_driver_install(&g_config, &t_config, &f_config);
    if (ret != ESP_OK) {
        ESP_LOGE(CAN_TAG, "Failed to install TWAI driver: %s", esp_err_to_name(ret));
        return ret;
    }
    
    canbus_initialized = true;
    ESP_LOGI(CAN_TAG, "CAN bus initialized successfully");
    return ESP_OK;
}

esp_err_t canbus_start(void)
{
    if (!canbus_initialized) {
        ESP_LOGE(CAN_TAG, "CAN bus not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (canbus_running) {
        ESP_LOGW(CAN_TAG, "CAN bus already running");
        return ESP_OK;
    }
    
    esp_err_t ret = twai_start();
    if (ret != ESP_OK) {
        ESP_LOGE(CAN_TAG, "Failed to start TWAI: %s", esp_err_to_name(ret));
        return ret;
    }
    
    canbus_running = true;
    ESP_LOGI(CAN_TAG, "CAN bus started successfully");
    return ESP_OK;
}

esp_err_t canbus_stop(void)
{
    if (!canbus_running) {
        ESP_LOGW(CAN_TAG, "CAN bus not running");
        return ESP_OK;
    }
    
    esp_err_t ret = twai_stop();
    if (ret != ESP_OK) {
        ESP_LOGE(CAN_TAG, "Failed to stop TWAI: %s", esp_err_to_name(ret));
        return ret;
    }
    
    canbus_running = false;
    ESP_LOGI(CAN_TAG, "CAN bus stopped");
    return ESP_OK;
}

// The canbus_task is now much simpler. It receives a message and passes it to the new parser.
void canbus_task(void *pvParameters)
{
    ESP_LOGI(CAN_TAG, "CAN bus task started");

    twai_message_t message;
    uint32_t consecutive_errors = 0;
    uint32_t last_message_time = xTaskGetTickCount();

    while (1) {
        esp_err_t ret = twai_receive(&message, pdMS_TO_TICKS(100)); // 100ms timeout

        if (ret == ESP_OK) {
            consecutive_errors = 0;
            last_message_time = xTaskGetTickCount();

            // 1. Parse the received CAN message using the new parser.
            // The parser will update the global g_ecu_data struct.
            parse_can_message(&message);

            // 2. The global data structure is now updated.
            // The UI task will periodically read this data to update the gauges.

            // 3. Send raw CAN message to Screen3 sniffer for debugging.
            ui_process_real_can_message(message.identifier, message.data, message.data_length_code);

            // 4. Log CAN trace to SD card if enabled
            /*
            // TODO: Re-implement CAN tracing with the new SD card driver
            if (sd_card_is_can_trace_enabled()) {
                char trace_buffer[100];
                // Format: timestamp,ID,d0,d1,d2,d3,d4,d5,d6,d7
                snprintf(trace_buffer, sizeof(trace_buffer), "%llu,%X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X\n",
                         esp_timer_get_time() / 1000,
                         message.identifier,
                         message.data[0], message.data[1], message.data[2], message.data[3],
                         message.data[4], message.data[5], message.data[6], message.data[7]);
                sd_card_append_file("/sdcard/can_trace.csv", trace_buffer);
            }
            */

        } else if (ret == ESP_ERR_TIMEOUT) {
            // Timeout is normal if there's no traffic on the bus.
            // Check if we haven't received data for too long.
            uint32_t current_time = xTaskGetTickCount();
            if ((current_time - last_message_time) > pdMS_TO_TICKS(5000)) { // 5 seconds
                ESP_LOGW(CAN_TAG, "No CAN messages received for 5 seconds");
                data_stream_add_entry("No CAN data received - check ECU connection", LOG_WARNING);
                last_message_time = current_time;
            }
        } else {
            consecutive_errors++;
            ESP_LOGW(CAN_TAG, "CAN receive error: %s (consecutive: %d)", esp_err_to_name(ret), consecutive_errors);
            data_stream_add_entry("CAN bus communication error", LOG_WARNING);

            if (consecutive_errors >= 5) {
                ESP_LOGE(CAN_TAG, "Too many consecutive CAN errors, attempting recovery");
                canbus_stop();
                vTaskDelay(pdMS_TO_TICKS(1000));
                esp_err_t restart_ret = canbus_start();
                if (restart_ret == ESP_OK) {
                    ESP_LOGI(CAN_TAG, "CAN bus restarted successfully");
                    data_stream_add_entry("CAN bus recovered", LOG_SUCCESS);
                    consecutive_errors = 0;
                } else {
                    ESP_LOGE(CAN_TAG, "Failed to restart CAN bus: %s", esp_err_to_name(restart_ret));
                    data_stream_add_entry("CAN bus recovery failed", LOG_ERROR);
                    vTaskDelay(pdMS_TO_TICKS(5000));
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
