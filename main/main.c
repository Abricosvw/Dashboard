/**
 * @file main.c
 * @brief Главный файл приложения ECU Dashboard.
 * 
 * ПОСЛЕДОВАТЕЛЬНОСТЬ ИНИЦИАЛИЗАЦИИ:
 * 1. NVS Flash, Background Task System
 * 2. WiFi, Web Server, CAN Bus
 * 3. Display Driver (создает I2C bus)  ← ВАЖНО: I2C bus доступен после этого!
 * 4. SD Card (использует I2C bus от display для CH422G)
 * 5. UI Tasks (update + delayed settings load)
 * 6. Console Task (I2C tools)
 * 
 * ПОСЛЕДОВАТЕЛЬНОСТЬ ЗАГРУЗКИ НАСТРОЕК:
 * 1. Интерфейс полностью загружается
 * 2. Задержка 2 секунды (для стабилизации UI)
 * 3. Чтение settings.cfg с SD карты (с мьютексом)
 * 4. Применение настроек к UI (с LVGL lock)
 * 
 * ПОСЛЕДОВАТЕЛЬНОСТЬ СОХРАНЕНИЯ НАСТРОЕК:
 * 1. Пользователь нажимает кнопку "Save Settings"
 * 2. trigger_settings_save() копирует настройки
 * 3. Background task получает задачу на запись
 * 4. Запись в settings.cfg (с мьютексом, ~100-200ms)
 * 5. Background task освобождает память
 * 
 * СКОРОСТЬ ЗАПИСИ SD КАРТЫ:
 * - Типичное время записи: 100-200 мс
 * - Мьютекс защищает от конкурентного доступа
 * - Background task не блокирует UI
 */

#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"

// New includes for I2C tools and SD card driver
#include "sd_card.h"
// Re-enabled I2C tools - conflict resolved with shared I2C bus
#include "cmd_i2ctools.h"
#include "esp_console.h"
#include "driver/i2c_master.h"


// Driver
// #include "driver/i2c.h" - Removed old I2C driver, using new i2c_master from display.h
#include "esp_lcd_touch_gt911.h"  // Re-enabled with compatibility wrapper

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
#include "../components/espressif__esp_lcd_touch/display.h"

// --- ДОБАВЛЕНО: Подключаем модуль фоновой задачи ---
#include "background_task.h"
// #include "sd_card_manager.h" - Removing the old SD card manager

static const char *TAG = "ECU_DASHBOARD";

// Forward declaration for the UI update task
void ui_update_task_handler(void *pvParameters);
void delayed_settings_load_task(void *pvParameters);

// Task to initialize and run the console - RE-ENABLED
// I2C API conflict resolved with shared bus approach
void console_task(void *pvParameters)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "ecu-dashboard>";

#if CONFIG_ESP_CONSOLE_UART
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
#elif CONFIG_ESP_CONSOLE_USB_CDC
    esp_console_dev_usb_cdc_config_t cdc_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&cdc_config, &repl_config, &repl));
#elif CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    esp_console_dev_usb_serial_jtag_config_t usbjtag_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&usbjtag_config, &repl_config, &repl));
#endif

    // I2C tools re-enabled - conflict resolved with shared I2C bus approach
    // Get the I2C bus handle from the display driver
    i2c_master_bus_handle_t bus_handle = display_get_i2c_bus_handle();
    if (bus_handle) {
        // Register I2C tool commands
        register_i2ctools(bus_handle);
        ESP_LOGI(TAG, "I2C tools registered successfully with shared bus");
    } else {
        ESP_LOGE(TAG, "Failed to get I2C bus handle, i2c-tools will not be available.");
    }


    printf("\n ==============================================================\n");
    printf(" |             Steps to Use i2c-tools                         |\n");
    printf(" |                                                            |\n");
    printf(" |  1. Try 'help', check all supported commands               |\n");
    printf(" |  2. Try 'i2cconfig' to configure your I2C bus              |\n");
    printf(" |  3. Try 'i2cdetect' to scan devices on the bus             |\n");
    printf(" |  4. Try 'i2cget' to get the content of specific register   |\n");
    printf(" |  5. Try 'i2cset' to set the value of specific register     |\n");
    printf(" |  6. Try 'i2cdump' to dump all the register (Experiment)    |\n");
    printf(" |                                                            |\n");
    printf(" ==============================================================\n\n");

    ESP_ERROR_CHECK(esp_console_start_repl(repl));

    vTaskDelete(NULL); // Should not get here
}


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

    // --- ДОБАВЛЕНО: Инициализация фоновой задачи для медленных операций ---
    // Эта задача будет обрабатывать медленные операции, такие как сохранение на SD-карту, не блокируя UI.
    background_task_init();

    // SD Card initialization moved after display driver to ensure I2C bus is available
    // This will be done after display() call

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
    display();

    // NOW initialize SD Card after I2C bus is available from display driver
    ESP_LOGI(TAG, "Initializing SD Card (after I2C bus setup)...");
    
    // Suspend GT911 polling during SD card initialization to avoid I2C conflicts
    extern volatile bool g_gt911_polling_suspended;
    g_gt911_polling_suspended = true;
    ESP_LOGI(TAG, "GT911 polling suspended for SD card initialization");
    
    esp_err_t sd_result = waveshare_sd_card_init();
    if (sd_result == ESP_OK) {
        ESP_LOGI(TAG, "SD Card initialized successfully");
        
        // Run diagnostic test
        ESP_LOGI(TAG, "Running SD card diagnostic test...");
        waveshare_sd_card_test();

        // Settings will be loaded AFTER UI is fully initialized (2 second delay)
        ESP_LOGI(TAG, "Settings will be loaded after UI initialization (2 sec delay)");
        
    } else {
        ESP_LOGE(TAG, "Failed to initialize SD Card! Error: %s (0x%x)", 
                 esp_err_to_name(sd_result), sd_result);
        ESP_LOGW(TAG, "System will continue without SD card functionality");
        
        // Initialize with default settings since SD card is not available
        ESP_LOGI(TAG, "Initializing with default settings");
        settings_load(); // This will handle default initialization internally
        
        // Resume GT911 polling immediately if SD card failed (no I2C operations ahead)
        g_gt911_polling_suspended = false;
        ESP_LOGI(TAG, "GT911 polling resumed (SD card initialization failed)");
    }
    
    // NOTE: GT911 polling will be resumed AFTER settings load completes (in delayed_settings_load_task)
    // This prevents I2C conflicts during UI initialization and settings loading

    // Create the UI update task
    xTaskCreate(ui_update_task_handler, "ui_update_task", 4096, NULL, 5, NULL);

    // Create delayed settings load task (waits 2 seconds after UI init)
    xTaskCreate(delayed_settings_load_task, "settings_load", 4096, NULL, 5, NULL);

    // Console task re-enabled - I2C API conflict resolved with shared bus
    xTaskCreate(console_task, "console_task", 4096, NULL, 10, NULL);

    // Проверяем границы всех экранов - все элементы должны быть внутри 800x480
    ESP_LOGI(TAG, "🔍 Проверка границ всех экранов...");
    ui_validate_all_screen_bounds();

    ESP_LOGI(TAG, "ECU Dashboard initialized. Connect to WiFi: ECU_Dashboard");
}
// Task to load settings with delay after UI initialization
void delayed_settings_load_task(void *pvParameters) {
    // Wait 2 seconds for UI to fully initialize
    ESP_LOGI(TAG, "⏳ Waiting 2 seconds for UI to fully initialize before loading settings...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Now load settings from SD card
    ESP_LOGI(TAG, "📂 Loading settings from SD card...");
    esp_err_t load_result = settings_load();
    
    if (load_result == ESP_OK) {
        ESP_LOGI(TAG, "✅ Settings loaded successfully, applying to UI...");
        
        // Apply loaded settings to UI (must be done with LVGL lock)
        if (example_lvgl_lock(-1)) {
            // Update Screen6 UI buttons to reflect loaded settings
            extern void ui_Screen6_update_button_states(void);
            ui_Screen6_update_button_states();
            
            // Apply demo mode and screen3 settings
            bool demo_enabled = demo_mode_get_enabled();
            bool screen3_enabled_flag = screen3_get_enabled();
            
            demo_mode_set_enabled(demo_enabled);
            screen3_set_enabled(screen3_enabled_flag);
            
            ESP_LOGI(TAG, "🎨 UI updated with loaded settings - Demo: %s, Screen3: %s",
                     demo_enabled ? "ON" : "OFF",
                     screen3_enabled_flag ? "ON" : "OFF");
            
            example_lvgl_unlock();
        }
    } else {
        ESP_LOGW(TAG, "⚠️ Failed to load settings, using defaults");
    }
    
    // Resume GT911 polling AFTER all I2C-intensive operations are complete
    extern volatile bool g_gt911_polling_suspended;
    extern esp_lcd_touch_handle_t tp;  // GT911 touch handle from display.h
    extern esp_err_t esp_lcd_touch_gt911_wake_up(esp_lcd_touch_handle_t tp);

    // Wake up GT911 and give it time to stabilize
    if (tp != NULL) {
        ESP_LOGI(TAG, "🔄 Waking up GT911 from suspension...");
        esp_lcd_touch_gt911_wake_up(tp);
        vTaskDelay(pdMS_TO_TICKS(50));  // Allow GT911 to fully wake up
        ESP_LOGI(TAG, "✅ GT911 wakeup complete");
    }
    
    g_gt911_polling_suspended = false;
    ESP_LOGI(TAG, "✅ GT911 polling resumed - touchscreen fully active");
    
    // Task finished, delete itself
    ESP_LOGI(TAG, "✓ Settings load task completed");
    vTaskDelete(NULL);
}

// Task to update the UI gauges periodically
void ui_update_task_handler(void *pvParameters) {
    while(1) {
        // Lock the LVGL mutex before touching UI elements
        if (example_lvgl_lock(-1)) {
            update_all_gauges();
            example_lvgl_unlock();
        }
        // Run this task at a reasonable rate, e.g., every 50ms (20 FPS)
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}