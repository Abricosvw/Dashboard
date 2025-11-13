#ifndef CAN_PARSER_H
#define CAN_PARSER_H

#include "driver/twai.h"

#ifdef __cplusplus
extern "C" {
#endif

// Function to parse a received CAN message and update the ECU data structure.
void parse_can_message(const twai_message_t* message);

// Function to set the configurable maximum torque value for calculations.
void can_parser_set_max_torque(float max_torque);

#ifdef __cplusplus
}
#endif

#endif // CAN_PARSER_H
