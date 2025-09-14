#ifndef SERIAL_READER_H
#define SERIAL_READER_H

#include <esp_err.h>
#include <freertos/queue.h>
#include "mre_data.h"

// Queue handle for passing MRE data to the UI task.
extern QueueHandle_t mre_data_queue;

// Initializes and creates the serial reader task.
esp_err_t serial_reader_task_init(void);

#endif // SERIAL_READER_H
