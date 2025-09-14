#include "include/can_parser.h"
#include "include/ecu_data.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "CAN_PARSER";

// Default max torque in Nm. Can be updated by can_parser_set_max_torque().
static float g_max_torque_nm = 500.0f;

// Helper function to extract a 16-bit unsigned integer from a byte array.
static inline uint16_t get_u16(const uint8_t* data, int offset) {
    return (uint16_t)(data[offset] << 8) | data[offset + 1];
}

void can_parser_set_max_torque(float max_torque) {
    if (max_torque > 0) {
        g_max_torque_nm = max_torque;
        ESP_LOGI(TAG, "Maximum torque for calculations set to %.1f Nm", g_max_torque_nm);
    }
}

void parse_can_message(const twai_message_t* message) {
    if (!message) {
        return;
    }

    // Get a pointer to the global ECU data struct.
    // The canbus_task is responsible for locking the mutex before calling this.
    ecu_data_t* ecu_data = ecu_data_get();
    if (!ecu_data) {
        return;
    }

    float raw_value_percent = 0.0f;

    switch (message->identifier) {
        case 0x280: // RPM, TPS, Pedal Pos, Target Torque, Actual Torque
            ecu_data->engine_rpm = ((message->data[2] << 8) | message->data[3]) * 0.25f;
            ecu_data->tps_position = message->data[7] * 0.3937f;
            ecu_data->abs_pedal_pos = message->data[4] * 0.4f;

            // Torque values are first calculated as %, then converted to Nm
            raw_value_percent = message->data[5] * 0.3937f;
            ecu_data->eng_trg_nm = (raw_value_percent / 100.0f) * g_max_torque_nm;

            // TODO: Resolve data conflict for Engine Actual Torque (eng_act_nm).
            // The user specification maps eng_act_nm to byte 3, but this byte is already
            // used as the low byte for the 16-bit engine_rpm value.
            // Disabling eng_act_nm parsing for now to prioritize engine_rpm.
            // raw_value_percent = message->data[3] * 0.3937f;
            // ecu_data->eng_act_nm = (raw_value_percent / 100.0f) * g_max_torque_nm;
            break;

        case 0x580: // MAP
            // Formula: raw * 0.01 = kPa
            ecu_data->map_kpa = ((message->data[2] << 8) | message->data[3]) * 0.01f;
            break;

        case 0x390: // Wastegate
            ecu_data->wg_set_percent = message->data[1] / 2.0f;
            ecu_data->wg_pos_percent = message->data[2] / 2.0f;
            break;

        case 0x394: // Blow-Off Valve
            ecu_data->bov_percent = (message->data[0] / 255.0f) * 50.0f;
            break;

        case 0x488: // TCU Torque
            raw_value_percent = message->data[1] * 0.39f;
            ecu_data->tcu_tq_req_nm = (raw_value_percent / 100.0f) * g_max_torque_nm;

            raw_value_percent = message->data[2] * 0.39f;
            ecu_data->tcu_tq_act_nm = (raw_value_percent / 100.0f) * g_max_torque_nm;
            break;

        case 0x288: // Torque Limit
            raw_value_percent = message->data[5] * 0.4f;
            ecu_data->limit_tq_nm = (raw_value_percent / 100.0f) * g_max_torque_nm;
            break;

        default:
            // Unhandled CAN ID
            break;
    }

    // After parsing and updating the local copy, update the global structure.
    // The calling function (canbus_task) is responsible for calling ecu_data_update.
}
