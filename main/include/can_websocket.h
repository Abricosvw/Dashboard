/*
 * CAN Data Server Header
 */

#ifndef CAN_WEBSOCKET_H
#define CAN_WEBSOCKET_H

#include "esp_err.h"
#include "esp_http_server.h" // Needed for httpd_handle_t

// Get current CAN data for debugging
typedef struct {
    uint16_t map_pressure;    // 100-250 kPa
    uint8_t  wastegate_pos;   // 0-100 %
    uint8_t  tps_position;    // 0-100 %
    uint16_t engine_rpm;      // 0-7000 RPM
    uint16_t target_boost;    // 100-250 kPa
    uint8_t  tcu_status;      // 0=OK, 1=WARN, 2=ERROR
    bool     data_valid;
} can_data_t;

/**
 * @brief Registers WebSocket and data handlers on an existing HTTP server.
 *
 * @param server The handle to the HTTP server instance.
 * @return esp_err_t ESP_OK on success, or an error code on failure.
 */
esp_err_t can_websocket_register_handlers(httpd_handle_t server);

// Update CAN data for broadcast
void update_websocket_can_data(uint16_t rpm, uint16_t map, uint8_t tps,
                              uint8_t wastegate, uint16_t target_boost, uint8_t tcu_status);

// Data broadcast task
void websocket_broadcast_task(void *pvParameters);

// Get current CAN data
can_data_t* get_can_data(void);

#endif // CAN_WEBSOCKET_H

