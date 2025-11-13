/**
 * @file main.c
 * @brief Главный файл приложения ECU Dashboard.
 */

#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"

// Driver
#include "driver/i2c_master.h"
#include "esp_lcd_touch_gt911.h"

// UI includes
#include "ui/ui.h"
#include "ui/settings_config.h"
#include "ui/ui_screen_manager.h"
#include "ui/ui_updates.h"
#include "web_server.h"

// CAN bus includes
#include "include/canbus.h"
#include "include/can_websocket.h"
#include "include/ecu_data.h"

// Display driver
// #include "../components/espressif__esp_lcd_touch/display.h" // Disabled: ESP32-S3 RGB panel specific

// I2C Tools
#include "../components/i2c_tools/main/cmd_i2ctools.h"
#include "../components/i2c_tools/main/ch422g.h"

// I2C bus handle definition
i2c_master_bus_handle_t tool_bus_handle;

// --- ДОБАВЛЕНО: Подключаем модуль фоновой задачи ---
#include "background_task.h"
#include "sd_card_manager.h"
#include "ui/settings_config.h"

// Enable CH422G support for SD card
#define CONFIG_USE_CH422G_FOR_SD 1

static const char *TAG = "ECU_DASHBOARD";

// Forward declaration for the UI update task
void ui_update_task_handler(void *pvParameters);

// Forward declaration for delayed settings load task
void delayed_settings_load_task(void *pvParameters);

void app_main(void)
{
    ESP_LOGI(TAG, "ECU Dashboard Starting...");
    ESP_LOGI(TAG, "Free heap: %ld bytes", esp_get_free_heap_size());

    // Initialize ECU data system
    ecu_data_init();
    system_settings_init();

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize I2C Master Bus
    ESP_LOGI(TAG, "Initializing I2C Master Bus...");
    
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = GPIO_NUM_4,  // SCL GPIO pin for ESP32-S3
        .sda_io_num = GPIO_NUM_5,  // SDA GPIO pin for ESP32-S3
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    
    esp_err_t i2c_ret = i2c_new_master_bus(&i2c_bus_config, &tool_bus_handle);
    if (i2c_ret == ESP_OK) {
        ESP_LOGI(TAG, "I2C Master Bus initialized successfully");
        
        // Initialize I2C tools (simplified version without console)
        register_i2ctools();
        
        // Optional: Scan for I2C devices
        i2c_scan_devices();
    } else {
        ESP_LOGE(TAG, "Failed to initialize I2C Master Bus: %s", esp_err_to_name(i2c_ret));
    }

    // Initialize CH422G GPIO Expander
    ESP_LOGI(TAG, "Initializing CH422G GPIO Expander...");
    esp_err_t ch422g_ret = ch422g_init(tool_bus_handle, CH422G_I2C_ADDR);
    if (ch422g_ret == ESP_OK) {
        ESP_LOGI(TAG, "CH422G initialized successfully");

        // Initialize LCD control through CH422G
        ESP_LOGI(TAG, "Initializing LCD control via CH422G...");
        ch422g_ret = ch422g_lcd_init();
        if (ch422g_ret == ESP_OK) {
            ESP_LOGI(TAG, "LCD control initialized successfully");
        } else {
            ESP_LOGE(TAG, "Failed to initialize LCD control: %s", esp_err_to_name(ch422g_ret));
        }

        // Initialize SD card control through CH422G
        ESP_LOGI(TAG, "Initializing SD card control via CH422G...");
        ch422g_ret = ch422g_sd_init();
        if (ch422g_ret == ESP_OK) {
            ESP_LOGI(TAG, "SD card control initialized successfully");

            // Configure SD card manager to use CH422G for CS control
            ESP_LOGI(TAG, "Configuring SD card manager to use CH422G...");
            esp_err_t sd_ch422g_ret = sd_card_use_ch422g_for_cs();
            if (sd_ch422g_ret == ESP_OK) {
                ESP_LOGI(TAG, "SD card manager now using CH422G for CS control");
            } else {
                ESP_LOGW(TAG, "Failed to configure SD card manager for CH422G: %s", esp_err_to_name(sd_ch422g_ret));
            }
        } else {
            ESP_LOGE(TAG, "Failed to initialize SD card control: %s", esp_err_to_name(ch422g_ret));
        }
    } else {
        ESP_LOGE(TAG, "Failed to initialize CH422G: %s", esp_err_to_name(ch422g_ret));
    }

    // --- ДОБАВЛЕНО: Инициализация фоновой задачи для медленных операций ---
    // Эта задача будет обрабатывать медленные операции, такие как сохранение на SD-карту, не блокируя UI.
    background_task_init();

    // Initialize SD Card
    ESP_LOGI(TAG, "Initializing SD Card...");
    esp_err_t sd_result = sd_card_init();
    if (sd_result == ESP_OK) {
        ESP_LOGI(TAG, "SD Card initialized successfully");
        
        // Get and display SD card info
        uint64_t total_bytes, free_bytes;
        if (sd_card_get_info(&total_bytes, &free_bytes) == ESP_OK) {
            ESP_LOGI(TAG, "SD Card Info - Total: %llu MB, Free: %llu MB",
                     total_bytes / (1024 * 1024), free_bytes / (1024 * 1024));
        }
        
        // Run diagnostic test on first boot
        sd_card_diagnostic_test();
        
        // Test SD card stability
        ESP_LOGI(TAG, "Testing SD card stability...");
        esp_err_t stability_result = sd_card_stability_test();
        if (stability_result == ESP_OK) {
            ESP_LOGI(TAG, "SD card connection is stable");
        } else if (stability_result == ESP_ERR_INVALID_RESPONSE) {
            ESP_LOGW(TAG, "SD card connection is unstable - consider checking hardware connections");
        } else {
            ESP_LOGE(TAG, "SD card connection is very unstable - hardware issues likely");
        }
        
        // Now that SD card is available, load settings
        ESP_LOGI(TAG, "Loading settings from SD card...");
        settings_load();
        settings_apply_changes();
        
        // Enable CAN trace logging from SD card settings if needed in the future
        // For now, let's enable it by default for testing.
        sd_card_set_can_trace_enabled(true);
    } else {
        ESP_LOGE(TAG, "Failed to initialize SD Card! Error: %s (0x%x)", 
                 esp_err_to_name(sd_result), sd_result);
        ESP_LOGW(TAG, "System will continue without SD card functionality");
        
        // Initialize with default settings since SD card is not available
        ESP_LOGI(TAG, "Initializing with default settings");
        settings_load(); // This will handle default initialization internally
    }
    
    // ВСЕГДА создаем отложенную задачу для загрузки настроек после полной инициализации системы
    // Даже если SD карта не инициализировалась при старте, она может стать доступной позже
    ESP_LOGI(TAG, "Creating delayed settings load task (will retry SD card initialization)...");
    xTaskCreate(delayed_settings_load_task, "delayed_settings_load", 4096, NULL, 1, NULL);

    // Initialize WiFi
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Start WiFi AP mode
    esp_netif_create_default_wifi_ap();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "ECU_Dashboard",
            .ssid_len = strlen("ECU_Dashboard"),
            .channel = 1,
            .password = "",
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi AP started. SSID: %s", wifi_config.ap.ssid);

    // Start web server with dashboard first (port 80)
    ESP_LOGI(TAG, "Starting web server...");
    esp_err_t web_ret = start_dashboard_web_server();
    if (web_ret == ESP_OK) {
        ESP_LOGI(TAG, "Web server started successfully!");
    } else {
        ESP_LOGE(TAG, "Failed to start web server: %s", esp_err_to_name(web_ret));
    }

    // Initialize CAN bus
    ESP_LOGI(TAG, "Initializing CAN bus...");
    esp_err_t can_ret = canbus_init();
    if (can_ret == ESP_OK) {
        ESP_LOGI(TAG, "CAN bus initialized successfully!");

        // Start CAN bus
        can_ret = canbus_start();
        if (can_ret == ESP_OK) {
            ESP_LOGI(TAG, "CAN bus started successfully!");

            // Create CAN task
            xTaskCreate(canbus_task, "can_task", 4096, NULL, 10, NULL);
            ESP_LOGI(TAG, "CAN task created");

            // Start WebSocket server for CAN data (port 8080)
            esp_err_t ws_ret = start_websocket_server();
            if (ws_ret == ESP_OK) {
                ESP_LOGI(TAG, "WebSocket server for CAN started successfully!");
                // Create WebSocket broadcast task
                xTaskCreate(websocket_broadcast_task, "ws_broadcast", 4096, NULL, 5, NULL);
            } else {
                ESP_LOGE(TAG, "Failed to start WebSocket server: %s", esp_err_to_name(ws_ret));
            }
        } else {
            ESP_LOGE(TAG, "Failed to start CAN bus: %s", esp_err_to_name(can_ret));
        }
    } else {
        ESP_LOGE(TAG, "Failed to initialize CAN bus: %s", esp_err_to_name(can_ret));
    }
    
    /* Initialize display and UI */
    // display(); // TODO: Implement display() function or use alternative initialization

    // Create the UI update task
    xTaskCreate(ui_update_task_handler, "ui_update_task", 4096, NULL, 5, NULL);

    // Проверяем границы всех экранов - все элементы должны быть внутри 800x480
    ESP_LOGI(TAG, "🔍 Проверка границ всех экранов...");
    ui_validate_all_screen_bounds();

    ESP_LOGI(TAG, "ECU Dashboard initialized. Connect to WiFi: ECU_Dashboard");
}
// Task to update the UI gauges periodically
void ui_update_task_handler(void *pvParameters) {
    while(1) {
        // Lock the LVGL mutex before touching UI elements
        // if (example_lvgl_lock(-1)) {
            update_all_gauges();
            // example_lvgl_unlock();
        // }
        // Run this task at a reasonable rate, e.g., every 50ms (20 FPS)
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// Отложенная задача для загрузки настроек после полной инициализации системы
void delayed_settings_load_task(void *pvParameters) {
    ESP_LOGI(TAG, "Starting delayed settings load task...");
    
    // Ждем 10 секунд после полной инициализации системы для стабилизации
    ESP_LOGI(TAG, "Waiting 10 seconds for system stabilization before attempting settings load...");
    vTaskDelay(pdMS_TO_TICKS(10000));
    
    ESP_LOGI(TAG, "Attempting delayed settings load from SD card...");
    
    // Попытка загрузить настройки несколько раз с интервалами - больше попыток!
    for (int attempt = 1; attempt <= 5; attempt++) {
        ESP_LOGI(TAG, "Delayed settings load attempt %d/5", attempt);
        
        // Попробуем инициализировать SD карту снова с большим таймаутом
        ESP_LOGI(TAG, "Attempting to initialize SD card for settings load...");
        esp_err_t sd_result = sd_card_init();
        if (sd_result == ESP_OK) {
            ESP_LOGI(TAG, "SD card initialized successfully for settings load");
            
            // Попробуем загрузить настройки
            esp_err_t load_result = settings_load();
            if (load_result == ESP_OK) {
                ESP_LOGI(TAG, "Settings loaded successfully from SD card on delayed attempt %d", attempt);
                
                // Применяем загруженные настройки к UI
                settings_apply_changes();
                ESP_LOGI(TAG, "Settings applied to UI successfully");
                
                break; // Успешно загружены, выходим из цикла
            } else {
                ESP_LOGW(TAG, "Failed to load settings on delayed attempt %d: %s", attempt, esp_err_to_name(load_result));
            }
        } else {
            ESP_LOGW(TAG, "SD card initialization failed on delayed attempt %d: %s", attempt, esp_err_to_name(sd_result));
        }
        
        if (attempt < 5) {
            int wait_time = 10000 + (5000 * attempt); // Задержка: 15, 20, 25, 30 сек
            ESP_LOGI(TAG, "Waiting %d seconds before next attempt...", wait_time/1000);
            vTaskDelay(pdMS_TO_TICKS(wait_time));
        }
    }
    
    ESP_LOGI(TAG, "Delayed settings load task completed");
    // Удаляем задачу после завершения
    vTaskDelete(NULL);
}