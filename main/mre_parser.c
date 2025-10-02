#include "include/mre_parser.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"

static const char *TAG = "MRE_PARSER";

// Default max torque in Nm. Can be updated by mre_set_max_torque().
// This seems to be a leftover from a previous implementation and is not used
// if the MRE sends torque values directly in Nm.
static float max_torque_nm = 500.0f;

void mre_set_max_torque(float new_max_torque) {
    if (new_max_torque > 0) {
        max_torque_nm = new_max_torque;
        ESP_LOGI(TAG, "Maximum torque set to %.1f Nm", max_torque_nm);
    }
}

float mre_get_max_torque(void) {
    return max_torque_nm;
}

esp_err_t mre_parse_data_string(char *raw_string, mre_data_t *data) {
    if (!raw_string || !data) {
        return ESP_ERR_INVALID_ARG;
    }

    // Use a copy of the string for strtok, as it modifies the original string.
    char *str_copy = strdup(raw_string);
    if (str_copy == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for string copy");
        return ESP_ERR_NO_MEM;
    }

    char *token;
    char *rest = str_copy;

    // Split the string by '|'
    while ((token = strtok_r(rest, "|", &rest))) {
        char *key = strtok_r(token, ":", &token);
        char *value_str = token;

        if (key && value_str) {
            float value = atof(value_str);

            // Assign values to the struct fields.
            // Assuming the MRE sends values in the correct units.
            if (strcmp(key, "RPM") == 0) data->rpm = (int)value;
            else if (strcmp(key, "TPS") == 0) data->tps = value;
            else if (strcmp(key, "AbsTPS") == 0) data->abs_tps = value;
            else if (strcmp(key, "MAP") == 0) data->map_kpa = value;
            else if (strcmp(key, "WG_SET") == 0) data->wg_set_percent = value;
            else if (strcmp(key, "WG_POS") == 0) data->wg_pos_percent = value;
            else if (strcmp(key, "TargetMAP") == 0) data->target_map_kpa = value;
            else if (strcmp(key, "BOV") == 0) data->bov_percent = value;
            else if (strcmp(key, "TCU_TQ") == 0) data->tcu_tq_nm = value;
            else if (strcmp(key, "TCU_ACT") == 0) data->tcu_act_nm = value;
            else if (strcmp(key, "Eng_TRG") == 0) data->eng_trg_nm = value;
            else if (strcmp(key, "Eng_ACT") == 0) data->eng_act_nm = value;
            else if (strcmp(key, "LimitTQ") == 0) data->limit_tq_nm = value;
            else if (strcmp(key, "PID") == 0) data->pid_correction = value;
        }
    }

    free(str_copy);
    return ESP_OK;
}
