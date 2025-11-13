/*
 * CAN WebSocket Server Header
 */

#ifndef CAN_WEBSOCKET_H
#define CAN_WEBSOCKET_H

#include "esp_err.h"

// Initialize and start WebSocket server
esp_err_t start_websocket_server(void);

// Stop WebSocket server
void stop_websocket_server(void);

// Update CAN data for WebSocket broadcast
void update_websocket_can_data(uint16_t rpm, uint16_t map, uint8_t tps, 
                              uint8_t wastegate, uint16_t target_boost, uint8_t tcu_status);

// WebSocket broadcast task
void websocket_broadcast_task(void *pvParameters);

#endif // CAN_WEBSOCKET_H