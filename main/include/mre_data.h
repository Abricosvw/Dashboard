#ifndef MRE_DATA_H
#define MRE_DATA_H

#include <stdint.h>

// This struct holds the parsed and converted data from the MRE serial string.
typedef struct {
    int rpm;
    float tps;
    float abs_tps;
    float map_kpa;
    float target_map_kpa;
    float wg_set_percent;
    float wg_pos_percent;
    float bov_percent;
    float tcu_tq_nm;
    float tcu_act_nm;
    float eng_trg_nm;
    float eng_act_nm;
    float limit_tq_nm;
    float pid_correction;
} mre_data_t;

#endif // MRE_DATA_H
