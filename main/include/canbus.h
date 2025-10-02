#ifndef CANBUS_H
#define CANBUS_H

#include "esp_err.h"
#include "driver/twai.h"
#include "include/ecu_data.h"

#ifdef __cplusplus
extern "C" {
#endif

// CAN bus configuration for ESP32-S3
#define CAN_TX_PIN GPIO_NUM_20  // TXD0 pin
#define CAN_RX_PIN GPIO_NUM_19  // RXD0 pin

// Standard ECU CAN IDs (example values)
#define CAN_ID_ENGINE_RPM     0x201
#define CAN_ID_MAP_PRESSURE   0x202
#define CAN_ID_TPS_POSITION   0x203
#define CAN_ID_WASTEGATE_POS  0x204
#define CAN_ID_TARGET_BOOST   0x205
#define CAN_ID_TCU_STATUS     0x206 // Example ID, not used by new parser

// Function prototypes
esp_err_t canbus_init(void);
esp_err_t canbus_start(void);
esp_err_t canbus_stop(void);
void canbus_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif // CANBUS_H
