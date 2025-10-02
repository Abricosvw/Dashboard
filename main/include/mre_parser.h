#ifndef MRE_PARSER_H
#define MRE_PARSER_H

#include "mre_data.h"
#include <esp_err.h>

// Function to parse the MRE data string and populate the data struct.
esp_err_t mre_parse_data_string(char *raw_string, mre_data_t *data);

// Function to set the configurable maximum torque value.
void mre_set_max_torque(float max_torque);

// Function to get the current maximum torque value.
float mre_get_max_torque(void);

#endif // MRE_PARSER_H
