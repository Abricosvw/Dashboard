#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_console.h"
#include "driver/i2c.h"
#include "esp_vfs_dev.h"
#include "driver/uart.h"
#include "linenoise/linenoise.h"

// Custom components
#include "display.h"
#include "sd_card_manager.h"
#include "i2c_utils.h"
#include "ch422g.h"

// UI and other services
#include "ui/ui.h"
#include "settings_config.h"
#include "ui/ui_updates.h"
#include "web_server.h"
#include "canbus.h"
#include "can_websocket.h"
#include "ecu_data.h"

static const char *TAG = "APP_MAIN";

// --- Global Handles for Shared Resources ---
ch422g_handle_t g_ch422g_handle = NULL;
// ---

void ui_update_task_handler(void *pvParameters);

static esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_HOST_SDA_IO,
        .scl_io_num = I2C_HOST_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_HOST_FREQ_HZ,
    };
    i2c_param_config(I2C_HOST_NUM, &conf);
    return i2c_driver_install(I2C_HOST_NUM, conf.mode, 0, 0, 0);
}

static void initialize_console(void)
{
    // Disable buffering on stdin and stdout
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    // Minicom, screen, idf_monitor send CR when ENTER key is pressed
    esp_vfs_dev_uart_port_set_rx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CR);
    // Move the caret to the beginning of the next line on '\n'
    esp_vfs_dev_uart_port_set_tx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CRLF);

    // Configure UART. Note that REF_TICK is used so that the baud rate remains
    // correct while APB frequency is changing in light sleep mode.
    const uart_config_t uart_config = {
            .baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .source_clk = UART_SCLK_DEFAULT,
    };
    // Install UART driver for console
    ESP_ERROR_CHECK(uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM, 256, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(CONFIG_ESP_CONSOLE_UART_NUM, &uart_config));

    // Tell VFS to use UART driver
    esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);

    esp_console_config_t console_config = {
            .max_cmdline_length = 256,
            .max_cmdline_args = 8,
    };
    ESP_ERROR_CHECK(esp_console_init(&console_config));

    // Configure linenoise line completion library
    linenoiseSetMultiLine(1);
    linenoiseSetCompletionCallback(&esp_console_get_completion);
    linenoiseSetHintsCallback((linenoiseHintsCallback*) &esp_console_get_hint);
    linenoiseHistorySetMaxLen(100);
    linenoiseSetMaxLineLen(console_config.max_cmdline_length);
}

void app_main(void)
{
    ESP_LOGI(TAG, "ECU Dashboard Starting...");

    ecu_data_init();
    system_settings_init();

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    initialize_console();

    ESP_LOGI(TAG, "Initializing I2C bus...");
    ESP_ERROR_CHECK(i2c_master_init());

    i2c_utils_register_commands();

    ESP_LOGI(TAG, "Initializing Display...");
    display_init(I2C_HOST_NUM);

    ESP_LOGI(TAG, "Initializing I/O Expander CH422G...");
    g_ch422g_handle = ch422g_init(I2C_HOST_NUM, CH422G_I2C_ADDRESS_0);
    if (g_ch422g_handle == NULL) {
        ESP_LOGE(TAG, "Failed to initialize CH422G expander! SD Card will not be available.");
    }

    if (g_ch422g_handle) {
        ESP_LOGI(TAG, "CH422G Initialized. Proceeding with SD Card...");
        if (sd_card_init() == ESP_OK) {
            ESP_LOGI(TAG, "SD Card initialized successfully.");
            settings_load();
        } else {
            ESP_LOGE(TAG, "Failed to initialize SD Card. Using default settings.");
            settings_load();
        }
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
        .ap = { .ssid = "ECU_Dashboard", .password = "", .max_connection = 4, .authmode = WIFI_AUTH_OPEN },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "WiFi AP started. SSID: ECU_Dashboard");

    start_dashboard_web_server();
    if (canbus_init() == ESP_OK && canbus_start() == ESP_OK) {
        xTaskCreate(canbus_task, "can_task", 4096, NULL, 10, NULL);
        if (start_websocket_server() == ESP_OK) {
            xTaskCreate(websocket_broadcast_task, "ws_broadcast", 4096, NULL, 5, NULL);
        }
    } else {
        ESP_LOGE(TAG, "Failed to start CAN bus");
    }

    xTaskCreate(ui_update_task_handler, "ui_update_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "ECU Dashboard initialization complete. Starting console REPL.");

    while(true) {
        const char* prompt = LOG_COLOR_I "esp> " LOG_RESET_COLOR;
        char* line = linenoise(prompt);
        if (line == NULL) {
            continue;
        }
        linenoiseHistoryAdd(line);
        int ret;
        esp_err_t err = esp_console_run(line, &ret);
        if (err == ESP_ERR_NOT_FOUND) {
            printf("Unrecognized command\n");
        } else if (err == ESP_ERR_INVALID_ARG) {
            // command was empty
        } else if (err == ESP_OK && ret != ESP_OK) {
            printf("Command returned non-zero error code: 0x%x (%s)\n", ret, esp_err_to_name(ret));
        } else if (err != ESP_OK) {
            printf("Internal error: %s\n", esp_err_to_name(err));
        }
        linenoiseFree(line);
    }
}

void ui_update_task_handler(void *pvParameters) {
    while(1) {
        if (example_lvgl_lock(-1)) {
            update_all_gauges();
            example_lvgl_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}