// ECU Dashboard Screen 6 - Device Parameters Settings
// Sixth screen dedicated to device configuration and settings

#include "../ui.h"
#include "ui_Screen6.h"
#include "ui_Screen2.h"
#include "ui_Screen1.h"
#include "ui_screen_manager.h"
#include "ui_helpers.h"
#include "ui_events.h"
#include "settings_config.h"      // Убедитесь, что этот файл подключен
#include "../background_task.h"  // Фоновая задача для асинхронных операций
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <esp_log.h>

LV_FONT_DECLARE(lv_font_montserrat_24);
LV_FONT_DECLARE(lv_font_montserrat_20);
LV_FONT_DECLARE(lv_font_montserrat_14);
LV_FONT_DECLARE(lv_font_montserrat_12);

// Screen object
lv_obj_t * ui_Screen6;

// Device Parameters Settings Objects
void * ui_Label_Device_Title;
void * ui_Button_Demo_Mode;
void * ui_Button_Enable_Screen3;
void * ui_Button_Save_Settings;
void * ui_Button_Reset_Settings;

// System statistics panel elements
static lv_obj_t * ui_SystemStats_Panel;
static lv_obj_t * ui_Label_CPU_Freq_Value;
static lv_obj_t * ui_Label_CPU_Load_Value;
static lv_obj_t * ui_Label_Task_Count_Value;
static lv_obj_t * ui_Label_Heap_Value;
static lv_obj_t * ui_Label_MinHeap_Value;
static lv_obj_t * ui_Label_Internal_Value;
static lv_obj_t * ui_Label_SPIRAM_Value;

// Settings state
static int settings_modified = 0;
static bool demo_mode_enabled = false;
static bool screen3_enabled = false;

// Function prototypes
static void swipe_handler_screen6(lv_event_t * e);
static void save_settings_event_cb(lv_event_t * e);
static void reset_settings_event_cb(lv_event_t * e);
static void demo_mode_event_cb(lv_event_t * e);
static void enable_screen3_event_cb(lv_event_t * e);

// Screen6 utility functions
void ui_Screen6_load_settings(void);
void ui_Screen6_save_settings(void);
void ui_Screen6_update_button_states(void);
void ui_save_device_settings(void);
void ui_reset_device_settings(void);


// Swipe handler for screen switching
static void swipe_handler_screen6(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    static lv_point_t start_point;
    static int is_swiping = 0;

    if (code == LV_EVENT_PRESSED) {
        lv_indev_t * indev = lv_indev_get_act();
        if (indev) {
            lv_indev_get_point(indev, &start_point);
            is_swiping = 1;
        }
    } else if (code == LV_EVENT_RELEASED && is_swiping != 0) {
        lv_point_t end_point;
        lv_indev_t * indev = lv_indev_get_act();
        if (indev) {
            lv_indev_get_point(indev, &end_point);
            int delta_x = end_point.x - start_point.x;

            if (delta_x > 50) {
                ui_switch_to_next_enabled_screen(false);
            }
            else if (delta_x < -50) {
                ui_switch_to_next_enabled_screen(true);
            }
        }
        is_swiping = 0;
    }
}

// Save settings button event callback
static void save_settings_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_save_device_settings();
        settings_modified = 0;
        ESP_LOGI("SCREEN6", "Device settings save triggered");
    }
}

// Reset settings button event callback
static void reset_settings_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_reset_device_settings();
        settings_modified = 0;
        ESP_LOGI("SCREEN6", "Device settings reset triggered");
    }
}

// Demo mode button event callback
static void demo_mode_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        demo_mode_enabled = !demo_mode_enabled;

        // Update button visuals on this screen
        ui_Screen6_update_button_states();

        // Update animations on all screens
        ui_set_global_demo_mode(demo_mode_enabled);

        settings_modified = 1;
        ESP_LOGI("SCREEN6", "Global demo mode toggled to: %s", demo_mode_enabled ? "ON" : "OFF");
    }
}

// Enable Screen3 button event callback
static void enable_screen3_event_cb(lv_event_t * e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        screen3_enabled = !screen3_enabled;
        ui_Screen6_update_button_states();
        settings_modified = 1;
        ESP_LOGI("SCREEN6", "Screen3 toggled to: %s", screen3_enabled ? "ON" : "OFF");
    }
}

static lv_obj_t * create_stats_row(lv_obj_t *parent, const char *title, lv_obj_t **value_label)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(row, 12, 0);
    lv_obj_set_style_pad_row(row, 4, 0);
    lv_obj_set_style_pad_left(row, 0, 0);
    lv_obj_set_style_pad_right(row, 0, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title_label = lv_label_create(row);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0x00D4FF), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);

    lv_obj_t *value = lv_label_create(row);
    lv_label_set_text(value, "—");
    lv_obj_set_style_text_color(value, lv_color_white(), 0);
    lv_obj_set_style_text_font(value, &lv_font_montserrat_14, 0);

    if (value_label) {
        *value_label = value;
    }

    // Increase intrinsic width to avoid clipping values
    lv_obj_set_width(value, 200);

    return row;
}

// Initialize Screen6
void ui_Screen6_screen_init(void)
{
    ui_Screen6 = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Screen6, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen6, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(ui_Screen6, LV_OPA_COVER, 0);

    // Main title
    ui_Label_Device_Title = lv_label_create(ui_Screen6);
    lv_label_set_text((lv_obj_t*)ui_Label_Device_Title, "Device Parameters Settings");
    lv_obj_set_style_text_color((lv_obj_t*)ui_Label_Device_Title, lv_color_hex(0x00D4FF), 0);
    lv_obj_set_style_text_font((lv_obj_t*)ui_Label_Device_Title, &lv_font_montserrat_24, 0);
    lv_obj_align((lv_obj_t*)ui_Label_Device_Title, LV_ALIGN_TOP_MID, 0, 20);

    // Demo Mode (Arcs) button - Top Left
    ui_Button_Demo_Mode = lv_btn_create(ui_Screen6);
    lv_obj_set_size((lv_obj_t*)ui_Button_Demo_Mode, 200, 50);
    lv_obj_align((lv_obj_t*)ui_Button_Demo_Mode, LV_ALIGN_TOP_LEFT, 50, 100);
    lv_obj_set_style_radius((lv_obj_t*)ui_Button_Demo_Mode, 25, 0);
    lv_obj_add_event_cb((lv_obj_t*)ui_Button_Demo_Mode, demo_mode_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * demo_label = lv_label_create((lv_obj_t*)ui_Button_Demo_Mode);
    lv_label_set_text(demo_label, "Demo Mode");
    lv_obj_set_style_text_color(demo_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(demo_label, &lv_font_montserrat_14, 0);
    lv_obj_center(demo_label);

    // Enable Screen3 button - Below Demo Mode
    ui_Button_Enable_Screen3 = lv_btn_create(ui_Screen6);
    lv_obj_set_size((lv_obj_t*)ui_Button_Enable_Screen3, 200, 50);
    lv_obj_align((lv_obj_t*)ui_Button_Enable_Screen3, LV_ALIGN_TOP_LEFT, 50, 170);
    lv_obj_set_style_radius((lv_obj_t*)ui_Button_Enable_Screen3, 25, 0);
    lv_obj_add_event_cb((lv_obj_t*)ui_Button_Enable_Screen3, enable_screen3_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * screen3_label = lv_label_create((lv_obj_t*)ui_Button_Enable_Screen3);
    lv_label_set_text(screen3_label, "Screen3");
    lv_obj_set_style_text_color(screen3_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(screen3_label, &lv_font_montserrat_14, 0);
    lv_obj_center(screen3_label);

    // Save Settings button - Below Screen3
    ui_Button_Save_Settings = lv_btn_create(ui_Screen6);
    lv_obj_set_size((lv_obj_t*)ui_Button_Save_Settings, 200, 50);
    lv_obj_align((lv_obj_t*)ui_Button_Save_Settings, LV_ALIGN_TOP_LEFT, 50, 240);
    lv_obj_set_style_bg_color((lv_obj_t*)ui_Button_Save_Settings, lv_color_hex(0x00FF88), 0);
    lv_obj_set_style_radius((lv_obj_t*)ui_Button_Save_Settings, 25, 0);
    lv_obj_add_event_cb((lv_obj_t*)ui_Button_Save_Settings, save_settings_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * save_label = lv_label_create((lv_obj_t*)ui_Button_Save_Settings);
    lv_label_set_text(save_label, "SAVE SETTINGS");
    lv_obj_set_style_text_color(save_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(save_label, &lv_font_montserrat_14, 0);
    lv_obj_center(save_label);

    // Reset Settings button - Bottom Left
    ui_Button_Reset_Settings = lv_btn_create(ui_Screen6);
    lv_obj_set_size((lv_obj_t*)ui_Button_Reset_Settings, 200, 50);
    lv_obj_align((lv_obj_t*)ui_Button_Reset_Settings, LV_ALIGN_TOP_LEFT, 50, 310);
    lv_obj_set_style_bg_color((lv_obj_t*)ui_Button_Reset_Settings, lv_color_hex(0xFF3366), 0);
    lv_obj_set_style_radius((lv_obj_t*)ui_Button_Reset_Settings, 25, 0);
    lv_obj_add_event_cb((lv_obj_t*)ui_Button_Reset_Settings, reset_settings_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * reset_label = lv_label_create((lv_obj_t*)ui_Button_Reset_Settings);
    lv_label_set_text(reset_label, "RESET SETTINGS");
    lv_obj_set_style_text_color(reset_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(reset_label, &lv_font_montserrat_14, 0);
    lv_obj_center(reset_label);

    // Add event handlers for swipe gestures
    lv_obj_add_event_cb(ui_Screen6, swipe_handler_screen6, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(ui_Screen6, swipe_handler_screen6, LV_EVENT_RELEASED, NULL);

    // Add standardized navigation buttons
    ui_create_standard_navigation_buttons(ui_Screen6);

    // Create system statistics panel on the right side
    ui_SystemStats_Panel = lv_obj_create(ui_Screen6);
    lv_obj_set_size(ui_SystemStats_Panel, 500, 350);
    lv_obj_align(ui_SystemStats_Panel, LV_ALIGN_TOP_RIGHT, -40, 90);
    lv_obj_set_style_bg_opa(ui_SystemStats_Panel, LV_OPA_20, 0);
    lv_obj_set_style_bg_color(ui_SystemStats_Panel, lv_color_hex(0x101921), 0);
    lv_obj_set_style_border_width(ui_SystemStats_Panel, 2, 0);
    lv_obj_set_style_border_color(ui_SystemStats_Panel, lv_color_hex(0x00D4FF), 0);
    lv_obj_set_style_radius(ui_SystemStats_Panel, 16, 0);
    lv_obj_set_style_pad_all(ui_SystemStats_Panel, 18, 0);
    lv_obj_set_style_pad_row(ui_SystemStats_Panel, 12, 0);
    lv_obj_set_style_pad_column(ui_SystemStats_Panel, 8, 0);
    lv_obj_set_scrollbar_mode(ui_SystemStats_Panel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(ui_SystemStats_Panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_SystemStats_Panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *stats_title = lv_label_create(ui_SystemStats_Panel);
    lv_label_set_text(stats_title, "System Diagnostics");
    lv_obj_set_style_text_color(stats_title, lv_color_hex(0x00D4FF), 0);
    lv_obj_set_style_text_font(stats_title, &lv_font_montserrat_20, 0);

    (void)create_stats_row(ui_SystemStats_Panel, "CPU Frequency:", &ui_Label_CPU_Freq_Value);
    (void)create_stats_row(ui_SystemStats_Panel, "CPU Load:", &ui_Label_CPU_Load_Value);
    (void)create_stats_row(ui_SystemStats_Panel, "Total Tasks:", &ui_Label_Task_Count_Value);
    (void)create_stats_row(ui_SystemStats_Panel, "Heap Usage:", &ui_Label_Heap_Value);
    (void)create_stats_row(ui_SystemStats_Panel, "Minimum Free Heap:", &ui_Label_MinHeap_Value);
    (void)create_stats_row(ui_SystemStats_Panel, "Internal RAM:", &ui_Label_Internal_Value);
    (void)create_stats_row(ui_SystemStats_Panel, "PSRAM:", &ui_Label_SPIRAM_Value);

    lv_obj_t *stats_hint = lv_label_create(ui_SystemStats_Panel);
    lv_label_set_text(stats_hint, "Updated every 5 seconds");
    lv_obj_set_style_text_color(stats_hint, lv_color_hex(0x7aa7c7), 0);
    lv_obj_set_style_text_font(stats_hint, &lv_font_montserrat_12, 0);

    // Load saved settings and update button states
    ui_Screen6_load_settings();
    ui_Screen6_update_button_states();
    ui_screen6_mark_stats_unavailable();
}

// Destroy Screen6
void ui_Screen6_screen_destroy(void)
{
    if(ui_Screen6) {
        lv_obj_del(ui_Screen6);
        ui_Screen6 = NULL;
    }
}

// Load Screen6 settings from the configuration system (which uses SD card)
void ui_Screen6_load_settings(void)
{
    demo_mode_enabled = demo_mode_get_enabled();
    screen3_enabled = screen3_get_enabled();
    ESP_LOGI("SCREEN6", "Loaded settings - Demo: %s, Screen3: %s",
             demo_mode_enabled ? "ON" : "OFF",
             screen3_enabled ? "ON" : "OFF");
}

// Save Screen6 settings to the SD Card via the configuration system
void ui_Screen6_save_settings(void)
{
    demo_mode_set_enabled(demo_mode_enabled);
    screen3_set_enabled(screen3_enabled);
    trigger_settings_save(); // Use the non-blocking trigger
    ESP_LOGI("SCREEN6", "Triggered save settings - Demo: %s, Screen3: %s",
             demo_mode_enabled ? "ON" : "OFF",
             screen3_enabled ? "ON" : "OFF");
}

// Update button states based on loaded settings
void ui_Screen6_update_button_states(void)
{
    lv_obj_t* btn;
    lv_obj_t* label;

    // Update Demo Mode button
    btn = (lv_obj_t*)ui_Button_Demo_Mode;
    if (btn) {
        label = lv_obj_get_child(btn, 0);
        if (demo_mode_enabled) {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x00FF88), 0);
            if (label) lv_label_set_text(label, "Demo Mode: ON");
        } else {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0xFF3366), 0);
            if (label) lv_label_set_text(label, "Demo Mode: OFF");
        }
    }

    // Update Screen3 button
    btn = (lv_obj_t*)ui_Button_Enable_Screen3;
    if (btn) {
        label = lv_obj_get_child(btn, 0);
        if (screen3_enabled) {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x00FF88), 0);
             if (label) lv_label_set_text(label, "Screen3: ON");
        } else {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0xFF3366), 0);
            if (label) lv_label_set_text(label, "Screen3: OFF");
        }
    }
}

// Save device settings (wrapper)
void ui_save_device_settings(void)
{
    ESP_LOGI("SCREEN6", "Saving device settings to SD Card");
    ui_Screen6_save_settings();
}

// =================================================================
// ИСПРАВЛЕННАЯ ФУНКЦИЯ / FIXED FUNCTION
// =================================================================
void ui_reset_device_settings(void)
{
    ESP_LOGI("SCREEN6", "Resetting device settings to defaults");
    
    // 1. Устанавливаем локальные переменные в значения по умолчанию
    // Вместо вызова settings_reset_to_defaults(), который может вызывать блокирующую запись,
    // мы просто меняем состояние здесь и затем вызываем нашу асинхронную функцию сохранения.
    demo_mode_enabled = DEFAULT_DEMO_MODE_ENABLED;
    screen3_enabled = DEFAULT_SCREEN3_ENABLED;

    // 2. Обновляем внешний вид кнопок на экране, чтобы отразить сброс
    ui_Screen6_update_button_states();

    // 3. Сохраняем эти новые значения по умолчанию на SD-карту асинхронно
    // Эта функция уже вызывает trigger_settings_save() и не блокирует UI.
    ui_Screen6_save_settings();
}

static void set_label_to_placeholder(lv_obj_t *label)
{
    if (label) {
        lv_label_set_text(label, "—");
    }
}

void ui_screen6_mark_stats_unavailable(void)
{
    set_label_to_placeholder(ui_Label_CPU_Freq_Value);
    set_label_to_placeholder(ui_Label_CPU_Load_Value);
    set_label_to_placeholder(ui_Label_Task_Count_Value);
    set_label_to_placeholder(ui_Label_Heap_Value);
    set_label_to_placeholder(ui_Label_MinHeap_Value);
    set_label_to_placeholder(ui_Label_Internal_Value);
    set_label_to_placeholder(ui_Label_SPIRAM_Value);
}

static void set_memory_label(lv_obj_t *label, size_t free_bytes, size_t total_bytes)
{
    if (!label) {
        return;
    }

    if (total_bytes == 0) {
        lv_label_set_text(label, "n/a");
        return;
    }

    size_t used_bytes = total_bytes > free_bytes ? total_bytes - free_bytes : 0;
    unsigned long used_kb = (unsigned long)(used_bytes / 1024);
    unsigned long total_kb = (unsigned long)(total_bytes / 1024);
    float percent_used = (total_bytes > 0) ? ((float)used_bytes * 100.0f / (float)total_bytes) : 0.0f;

    lv_label_set_text_fmt(label, "%lu/%lu KB (%.1f%% used)",
                          used_kb, total_kb, percent_used);
}

void ui_screen6_set_system_stats(const ui_system_stats_t *stats)
{
    if (!stats || !ui_SystemStats_Panel) {
        ESP_LOGW("SCREEN6", "Stats update called but stats=%p, panel=%p", stats, ui_SystemStats_Panel);
        return;
    }

    ESP_LOGD("SCREEN6", "Updating stats - CPU: %.1f MHz, Load: %.1f%%, Tasks: %lu", 
             stats->cpu_freq_mhz, stats->cpu_load_percent, (unsigned long)stats->task_count);

    static float last_cpu_freq = -1.0f;
    static float last_cpu_load = -1.0f;

    if (ui_Label_CPU_Freq_Value) {
        if (stats->cpu_freq_mhz >= 0.0f) {
            if ((last_cpu_freq < 0.0f) || (fabsf(stats->cpu_freq_mhz - last_cpu_freq) > 0.05f)) {
                lv_label_set_text_fmt(ui_Label_CPU_Freq_Value, "%.1f MHz", stats->cpu_freq_mhz);
                last_cpu_freq = stats->cpu_freq_mhz;
            }
        } else if (last_cpu_freq >= 0.0f) {
            lv_label_set_text(ui_Label_CPU_Freq_Value, "n/a");
            last_cpu_freq = -1.0f;
        }
    } else {
        ESP_LOGW("SCREEN6", "ui_Label_CPU_Freq_Value is NULL!");
    }

    if (ui_Label_CPU_Load_Value) {
        if (stats->cpu_load_percent >= 0.0f) {
            if ((last_cpu_load < 0.0f) || (fabsf(stats->cpu_load_percent - last_cpu_load) > 0.05f)) {
                lv_label_set_text_fmt(ui_Label_CPU_Load_Value, "%.1f%%", stats->cpu_load_percent);
                ESP_LOGD("SCREEN6", "Set CPU load label to %.1f%%", stats->cpu_load_percent);
                last_cpu_load = stats->cpu_load_percent;
            }
        } else if (last_cpu_load >= 0.0f) {
            lv_label_set_text(ui_Label_CPU_Load_Value, "n/a");
            last_cpu_load = -1.0f;
        }
    } else {
        ESP_LOGW("SCREEN6", "ui_Label_CPU_Load_Value is NULL!");
    }

    if (ui_Label_Task_Count_Value) {
        lv_label_set_text_fmt(ui_Label_Task_Count_Value, "%lu tasks",
                              (unsigned long)stats->task_count);
    }

    if (ui_Label_Heap_Value) {
        set_memory_label(ui_Label_Heap_Value, stats->heap_free_bytes, stats->heap_total_bytes);
    }

    if (ui_Label_MinHeap_Value) {
        unsigned long min_free_kb = (unsigned long)(stats->heap_min_free_bytes / 1024);
        lv_label_set_text_fmt(ui_Label_MinHeap_Value, "%lu KB since boot", min_free_kb);
    }

    if (ui_Label_Internal_Value) {
        set_memory_label(ui_Label_Internal_Value, stats->internal_free_bytes, stats->internal_total_bytes);
    }

    if (ui_Label_SPIRAM_Value) {
        if (stats->spiram_total_bytes > 0) {
            set_memory_label(ui_Label_SPIRAM_Value, stats->spiram_free_bytes, stats->spiram_total_bytes);
        } else {
            lv_label_set_text(ui_Label_SPIRAM_Value, "Not detected");
        }
    }
}
