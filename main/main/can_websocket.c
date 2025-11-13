
/*
 * CAN WebSocket Server for ECU Dashboard
 * Broadcasts CAN data over WebSocket for Android/Web clients
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "driver/twai.h"
#include <string.h>
#include "esp_system.h"
#include "include/can_websocket.h"
#include "ui/settings_config.h"

static const char *TAG = "CAN_WEBSOCKET";

// Forward declarations
static esp_err_t ws_handler(httpd_req_t *req);
static esp_err_t data_handler(httpd_req_t *req);

// Global CAN data (moved to header for external access)
static can_data_t g_can_data = {0};
static httpd_handle_t ws_server = NULL;

// WebSocket handler for basic HTTP upgrade
static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Handshake done, new WebSocket connection opened");
        return ESP_OK;
    }
    
    // For now, just return OK since we're not handling complex WebSocket frames
    return ESP_OK;
}

// Data handler for /data endpoint
static esp_err_t data_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        // Check if demo mode is enabled
        bool demo_enabled = demo_mode_get_enabled();
        ESP_LOGI(TAG, "🔌 WebSocket server - demo mode check: %s", demo_enabled ? "ENABLED" : "DISABLED");

        if (demo_enabled) {
            // Demo mode enabled - return real or simulated data
            if (g_can_data.data_valid) {
                char json_data[256];
                snprintf(json_data, sizeof(json_data),
                    "{\"map_pressure\":%d,\"wastegate_pos\":%d,\"tps_position\":%d,"
                    "\"engine_rpm\":%d,\"target_boost\":%d,\"tcu_status\":%d}",
                    g_can_data.map_pressure, g_can_data.wastegate_pos, g_can_data.tps_position,
                    g_can_data.engine_rpm, g_can_data.target_boost, g_can_data.tcu_status);

                httpd_resp_set_type(req, "application/json");
                httpd_resp_send(req, json_data, HTTPD_RESP_USE_STRLEN);
                ESP_LOGD(TAG, "CAN data sent (demo mode)");
            } else {
                // No real data, generate demo data
                static int demo_counter = 0;
                demo_counter++;

                // Generate demo data using simple periodic functions
                int cycle = demo_counter % 100;
                float phase = cycle / 100.0f;
                float map_pressure = 120.0f + 30.0f * (cycle > 50 ? (100 - cycle) : cycle) / 50.0f;
                float wastegate_pos = 45.0f + 25.0f * phase;
                float tps_position = 35.0f + 30.0f * phase;
                float engine_rpm = 2500.0f + 500.0f * phase;
                float target_boost = 180.0f + 20.0f * phase;
                int tcu_status = (demo_counter % 100 > 95) ? 1 : 0;

                char json_data[256];
                snprintf(json_data, sizeof(json_data),
                    "{\"map_pressure\":%.1f,\"wastegate_pos\":%.1f,\"tps_position\":%.1f,"
                    "\"engine_rpm\":%.0f,\"target_boost\":%.1f,\"tcu_status\":%d}",
                    map_pressure, wastegate_pos, tps_position, engine_rpm, target_boost, tcu_status);

                httpd_resp_set_type(req, "application/json");
                httpd_resp_send(req, json_data, HTTPD_RESP_USE_STRLEN);
                ESP_LOGD(TAG, "Demo CAN data sent");
            }
        } else {
            // Demo mode disabled - return zero values
            char json_data[256];
            snprintf(json_data, sizeof(json_data),
                "{\"map_pressure\":0.0,\"wastegate_pos\":0.0,\"tps_position\":0.0,"
                "\"engine_rpm\":0,\"target_boost\":0.0,\"tcu_status\":0}");

            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, json_data, HTTPD_RESP_USE_STRLEN);
            ESP_LOGI(TAG, "❌ WebSocket demo mode disabled, zero values sent to client");
        }
        return ESP_OK;
    }

    return ESP_FAIL;
}



// Broadcast CAN data to all connected clients via HTTP
void broadcast_can_data(void)
{
    bool demo_enabled = demo_mode_get_enabled();
    ESP_LOGD(TAG, "📡 Broadcast check - demo mode: %s", demo_enabled ? "ENABLED" : "DISABLED");

    if (!demo_enabled) {
        // Demo mode disabled - don't broadcast
        ESP_LOGI(TAG, "🚫 Broadcast cancelled - demo mode disabled");
        return;
    }

    if (!g_can_data.data_valid) {
        // No real data, generate demo data for broadcast
        static int demo_counter = 0;
        demo_counter++;

        // Generate demo data using simple periodic functions
        int cycle = demo_counter % 100;
        float phase = cycle / 100.0f;
        float map_pressure = 120.0f + 30.0f * (cycle > 50 ? (100 - cycle) : cycle) / 50.0f;
        float wastegate_pos = 45.0f + 25.0f * phase;
        float tps_position = 35.0f + 30.0f * phase;
        float engine_rpm = 2500.0f + 500.0f * phase;
        float target_boost = 180.0f + 20.0f * phase;
        int tcu_status = (demo_counter % 100 > 95) ? 1 : 0;

        char json_data[256];
        snprintf(json_data, sizeof(json_data),
            "{\"map_pressure\":%.1f,\"wastegate_pos\":%.1f,\"tps_position\":%.1f,"
            "\"engine_rpm\":%.0f,\"target_boost\":%.1f,\"tcu_status\":%d}",
            map_pressure, wastegate_pos, tps_position, engine_rpm, target_boost, tcu_status);

        ESP_LOGD(TAG, "Broadcasting demo CAN data: %s", json_data);
        return;
    }

    char json_data[256];
    snprintf(json_data, sizeof(json_data),
        "{\"map_pressure\":%d,\"wastegate_pos\":%d,\"tps_position\":%d,"
        "\"engine_rpm\":%d,\"target_boost\":%d,\"tcu_status\":%d}",
        g_can_data.map_pressure, g_can_data.wastegate_pos, g_can_data.tps_position,
        g_can_data.engine_rpm, g_can_data.target_boost, g_can_data.tcu_status);

    ESP_LOGD(TAG, "Broadcasting CAN data: %s", json_data);
}

// Update CAN data from main CAN task
void update_websocket_can_data(uint16_t rpm, uint16_t map, uint8_t tps,
                              uint8_t wastegate, uint16_t target_boost, uint8_t tcu_status)
{
    bool demo_enabled = demo_mode_get_enabled();
    ESP_LOGD(TAG, "🔄 Update check - demo mode: %s", demo_enabled ? "ENABLED" : "DISABLED");

    if (demo_enabled) {
        // Only update if demo mode is enabled
        g_can_data.engine_rpm = rpm;
        g_can_data.map_pressure = map;
        g_can_data.tps_position = tps;
        g_can_data.wastegate_pos = wastegate;
        g_can_data.target_boost = target_boost;
        g_can_data.tcu_status = tcu_status;
        g_can_data.data_valid = true;

        ESP_LOGD(TAG, "✅ CAN data updated - RPM: %d, MAP: %d, TPS: %d, Wastegate: %d, Boost: %d, TCU: %d",
                  rpm, map, tps, wastegate, target_boost, tcu_status);

        // Broadcast updated data
        broadcast_can_data();
    } else {
        // Demo mode disabled - clear data
        g_can_data.engine_rpm = 0;
        g_can_data.map_pressure = 0;
        g_can_data.tps_position = 0;
        g_can_data.wastegate_pos = 0;
        g_can_data.target_boost = 0;
        g_can_data.tcu_status = 0;
        g_can_data.data_valid = false;

        ESP_LOGI(TAG, "🧹 Demo mode disabled, CAN data cleared to zero");
    }
}

// Start WebSocket server (simplified HTTP server for now)
esp_err_t start_websocket_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 8081;  // Use different port to avoid conflict with main HTTP server
    config.max_open_sockets = 7;

    ESP_LOGI(TAG, "Starting WebSocket server on port: '%d'", config.server_port);
    
    if (httpd_start(&ws_server, &config) == ESP_OK) {
        httpd_uri_t ws_uri = {
            .uri = "/ws",
            .method = HTTP_GET,
            .handler = ws_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(ws_server, &ws_uri);
        
        // Add data endpoint
        httpd_uri_t data_uri = {
            .uri = "/data",
            .method = HTTP_GET,
            .handler = data_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(ws_server, &data_uri);
        
        ESP_LOGI(TAG, "WebSocket server started");
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "Error starting WebSocket server");
    return ESP_FAIL;
}

// Stop WebSocket server
void stop_websocket_server(void)
{
    if (ws_server) {
        httpd_stop(ws_server);
        ws_server = NULL;
    }
}

// WebSocket broadcast task
void websocket_broadcast_task(void *pvParameters)
{
    while (1) {
        broadcast_can_data();
        vTaskDelay(pdMS_TO_TICKS(100)); // Broadcast at 10Hz
    }
}

// Get current CAN data for external access
can_data_t* get_can_data(void)
{
    return &g_can_data;
}

