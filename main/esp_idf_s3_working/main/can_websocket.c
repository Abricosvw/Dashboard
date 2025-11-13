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

static const char *TAG = "CAN_WEBSOCKET";

// WebSocket frame opcodes
#define WS_OPCODE_CONT    0x00
#define WS_OPCODE_TEXT    0x01
#define WS_OPCODE_BINARY  0x02
#define WS_OPCODE_CLOSE   0x08
#define WS_OPCODE_PING    0x09
#define WS_OPCODE_PONG    0x0A

// WebSocket structure
typedef struct {
    httpd_handle_t hd;
    int fd;
} async_resp_arg_t;

// Global CAN data
typedef struct {
    uint16_t map_pressure;    // 100-250 kPa
    uint8_t  wastegate_pos;   // 0-100 %
    uint8_t  tps_position;    // 0-100 %
    uint16_t engine_rpm;      // 0-7000 RPM
    uint16_t target_boost;    // 100-250 kPa
    uint8_t  tcu_status;      // 0=OK, 1=WARN, 2=ERROR
    bool     data_valid;
} can_data_t;

static can_data_t g_can_data = {0};
static httpd_handle_t ws_server = NULL;

// WebSocket send frame function
static esp_err_t ws_send_frame(httpd_req_t *req, const char *data, size_t len)
{
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t*)data;
    ws_pkt.len = len;
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    
    return httpd_ws_send_frame(req, &ws_pkt);
}

// WebSocket handler
static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Handshake done, new WebSocket connection opened");
        return ESP_OK;
    }
    
    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    
    // Set max_len to receive full frame
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }
    
    if (ws_pkt.len) {
        buf = calloc(1, ws_pkt.len + 1);
        if (buf == NULL) {
            ESP_LOGE(TAG, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }
        ESP_LOGI(TAG, "Got packet with message: %s", ws_pkt.payload);
    }
    
    // Send current CAN data as response
    if (g_can_data.data_valid) {
        char json_response[256];
        snprintf(json_response, sizeof(json_response),
            "{\"map_pressure\":%d,\"wastegate_pos\":%d,\"tps_position\":%d,"
            "\"engine_rpm\":%d,\"target_boost\":%d,\"tcu_status\":%d}",
            g_can_data.map_pressure, g_can_data.wastegate_pos, g_can_data.tps_position,
            g_can_data.engine_rpm, g_can_data.target_boost, g_can_data.tcu_status);
        
        ws_send_frame(req, json_response, strlen(json_response));
    }
    
    free(buf);
    return ret;
}

// Broadcast CAN data to all connected WebSocket clients
void broadcast_can_data(void)
{
    if (!ws_server || !g_can_data.data_valid) {
        return;
    }
    
    char json_data[256];
    snprintf(json_data, sizeof(json_data),
        "{\"map_pressure\":%d,\"wastegate_pos\":%d,\"tps_position\":%d,"
        "\"engine_rpm\":%d,\"target_boost\":%d,\"tcu_status\":%d}",
        g_can_data.map_pressure, g_can_data.wastegate_pos, g_can_data.tps_position,
        g_can_data.engine_rpm, g_can_data.target_boost, g_can_data.tcu_status);
    
    // Broadcast to all connected clients
    size_t clients = CONFIG_LWIP_MAX_SOCKETS;
    int client_fds[CONFIG_LWIP_MAX_SOCKETS];
    
    if (httpd_get_client_list(ws_server, &clients, client_fds) == ESP_OK) {
        for (size_t i = 0; i < clients; i++) {
            httpd_ws_frame_t ws_pkt;
            memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
            ws_pkt.payload = (uint8_t*)json_data;
            ws_pkt.len = strlen(json_data);
            ws_pkt.type = HTTPD_WS_TYPE_TEXT;
            
            httpd_ws_send_frame_async(ws_server, client_fds[i], &ws_pkt);
        }
    }
}

// Update CAN data from main CAN task
void update_websocket_can_data(uint16_t rpm, uint16_t map, uint8_t tps, 
                              uint8_t wastegate, uint16_t target_boost, uint8_t tcu_status)
{
    g_can_data.engine_rpm = rpm;
    g_can_data.map_pressure = map;
    g_can_data.tps_position = tps;
    g_can_data.wastegate_pos = wastegate;
    g_can_data.target_boost = target_boost;
    g_can_data.tcu_status = tcu_status;
    g_can_data.data_valid = true;
    
    // Broadcast updated data
    broadcast_can_data();
}

// Start WebSocket server
esp_err_t start_websocket_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 7;
    
    ESP_LOGI(TAG, "Starting WebSocket server on port: '%d'", config.server_port);
    
    if (httpd_start(&ws_server, &config) == ESP_OK) {
        httpd_uri_t ws_uri = {
            .uri = "/ws",
            .method = HTTP_GET,
            .handler = ws_handler,
            .user_ctx = NULL,
            .is_websocket = true
        };
        httpd_register_uri_handler(ws_server, &ws_uri);
        ESP_LOGI(TAG, "WebSocket server started");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Error starting WebSocket server");
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