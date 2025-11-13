#ifndef ECU_DATA_H
#define ECU_DATA_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

// ECU Data structure based on VW CAN bus spec
typedef struct {
    // Engine Parameters
    float engine_rpm;
    float tps_position;
    float abs_pedal_pos;
    float map_kpa;

    // Boost Control
    float wg_set_percent;
    float wg_pos_percent;
    float bov_percent;

    // Torque Values (Nm)
    float tcu_tq_req_nm;
    float tcu_tq_act_nm;
    float eng_trg_nm;
    float eng_act_nm;
    float limit_tq_nm;

    // System
    uint64_t timestamp;
} ecu_data_t;

// System settings
typedef struct {
    float max_boost_limit;       // Maximum boost limit
    float max_rpm_limit;         // Maximum RPM limit
    bool audio_alerts_enabled;   // Audio alerts enabled
    char ecu_address[32];        // ECU address
} system_settings_t;

// Connection status
typedef struct {
    bool connected;
    char message[128];
} connection_status_t;

// Data stream entry
typedef enum {
    LOG_INFO,
    LOG_WARNING,
    LOG_SUCCESS,
    LOG_ERROR
} log_type_t;

typedef struct {
    uint64_t timestamp;
    char message[256];
    log_type_t type;
} data_stream_entry_t;

// Function prototypes
void ecu_data_init(void);
void ecu_data_update(ecu_data_t *data);
// ✅ REMOVED: ecu_data_get() - was unsafe (no mutex protection)
void ecu_data_get_copy(ecu_data_t *data_copy); // Thread-safe getter - USE THIS!
// ✅ REMOVED: ecu_data_to_json(), ecu_data_from_json(), ecu_data_to_string() - were unused stubs
void ecu_data_simulate(ecu_data_t *data);

// System settings functions
void system_settings_init(void);
system_settings_t* system_settings_get(void);
void system_settings_save(const system_settings_t *settings);

// Logging functions
void data_stream_add_entry(const char *message, log_type_t type);
void data_stream_clear(void);
char* data_stream_to_json(void);
char* data_stream_to_string(void); // For simple logging/debugging

#ifdef __cplusplus
}
#endif

#endif // ECU_DATA_H
