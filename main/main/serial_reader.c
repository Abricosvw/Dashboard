#include <stdlib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <driver/uart.h>
#include <esp_log.h>
#include <esp_err.h>
#include "include/serial_reader.h"
#include "include/mre_parser.h"

static const char *TAG = "SERIAL_READER";

#define UART_NUM UART_NUM_1
#define UART_RX_BUF_SIZE 1024
#define MRE_DATA_QUEUE_SIZE 5

// Define the queue handle
QueueHandle_t mre_data_queue;

static void serial_reader_task(void *pvParameters) {
    uint8_t* data = (uint8_t*) malloc(UART_RX_BUF_SIZE);
    mre_data_t parsed_data;

    while (1) {
        int len = uart_read_bytes(UART_NUM, data, (UART_RX_BUF_SIZE - 1), 20 / portTICK_PERIOD_MS);
        if (len > 0) {
            data[len] = '\0';
            ESP_LOGD(TAG, "Received raw string: %s", (const char*)data);

            // Parse the string
            if (mre_parse_data_string((char*)data, &parsed_data) == ESP_OK) {
                // Send the parsed data to the queue, overwrite if full.
                if (xQueueSend(mre_data_queue, &parsed_data, (TickType_t)0) != pdPASS) {
                    ESP_LOGW(TAG, "MRE data queue full. Overwriting oldest data.");
                    xQueueOverwrite(mre_data_queue, &parsed_data);
                }
            } else {
                ESP_LOGE(TAG, "Failed to parse MRE data string.");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // Yield to other tasks
    }
    free(data);
}

esp_err_t serial_reader_task_init(void) {
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(UART_NUM, UART_RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // Create the queue to hold the MRE data
    mre_data_queue = xQueueCreate(MRE_DATA_QUEUE_SIZE, sizeof(mre_data_t));
    if (mre_data_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create MRE data queue");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Creating serial reader task");
    // Pin the task to Core 1 to keep it away from the UI task on Core 0
    xTaskCreatePinnedToCore(serial_reader_task, "serial_reader_task", 4096, NULL, 5, NULL, 1);

    return ESP_OK;
}
