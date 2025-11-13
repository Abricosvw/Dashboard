/*
 * ECU Data Structures and Definitions
 * ESP-IDF Component for ESP32-S3
 */

#ifndef ECU_S3_DATA_H
#define ECU_S3_DATA_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ECU Data Structure */
typedef struct {
    // Engine parameters
    uint16_t rpm;                    // Engine RPM (0-7000)
    uint16_t map_pressure;           // Manifold Absolute Pressure (kPa)
    uint8_t tps_position;            // Throttle Position Sensor (0-100%)
    uint8_t engine_load;             // Engine load (0-100%)
    int8_t coolant_temp;             // Coolant temperature (-40 to +215°C)
    
    // Boost control
    uint8_t wastegate_position;      // Wastegate position (0-100%)
    uint16_t target_boost;           // Target boost pressure (kPa)
    uint8_t boost_duty;              // Boost control duty cycle (0-100%)
    int8_t boost_error;              // Boost error (-128 to +127 kPa)
    
    // TCU (Transmission Control Unit)
    uint16_t torque_request;         // Torque request (Nm)
    bool tcu_protection;             // Protection mode active
    bool tcu_limp_mode;              // Limp mode active
    uint8_t gear_position;           // Current gear (0-8)
    
    // System status
    uint64_t last_update;            // Last update timestamp (ms)
    bool can_connected;              // CAN bus connection status
} ecu_s3_data_t;

/* CAN Message IDs */
#define CAN_ID_ECU_DATA         0x380    // ECU basic data
#define CAN_ID_TCU_DATA         0x440    // TCU status data
#define CAN_ID_BOOST_CONTROL    0x200    // Boost control data

/* Default values */
#define ECU_DEFAULT_IDLE_RPM    800
#define ECU_DEFAULT_ATMO_MAP    100      // kPa
#define ECU_DEFAULT_TARGET_BOOST 150     // kPa

/* Warning thresholds */
#define ECU_WARNING_HIGH_RPM    6500
#define ECU_WARNING_HIGH_MAP    230      // kPa
#define ECU_WARNING_HIGH_TPS    95       // %
#define ECU_CRITICAL_HIGH_MAP   250      // kPa

#ifdef __cplusplus
}
#endif

#endif /* ECU_S3_DATA_H */