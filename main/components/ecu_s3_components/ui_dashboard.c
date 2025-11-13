/*
 * ECU Dashboard UI Implementation for ESP32-S3 7" Display
 * ESP-IDF Component with LVGL 8.3.x
 */

#include "ui_dashboard.h"
#include "ecu_data.h"
#include <esp_log.h>
#include <string.h>
#include <stdio.h>

static const char* TAG = "UI_DASHBOARD_S3";

/* UI Objects */
static lv_obj_t *main_screen;
static lv_obj_t *header_panel;
static lv_obj_t *gauge_panel;
static lv_obj_t *status_panel;
static lv_obj_t *footer_panel;

/* Gauge objects */
static lv_obj_t *gauge_rpm;
static lv_obj_t *gauge_map;
static lv_obj_t *gauge_tps;
static lv_obj_t *gauge_wastegate;
static lv_obj_t *gauge_target_boost;
static lv_obj_t *gauge_coolant_temp;

/* Status objects */
static lv_obj_t *label_tcu_status;
static lv_obj_t *label_connection;
static lv_obj_t *label_wifi_status;
static lv_obj_t *label_gear;
static lv_obj_t *label_engine_load;

/* Header objects */
static lv_obj_t *label_title;
static lv_obj_t *label_time;

/* Create automotive styled gauge for 7" display */
static lv_obj_t* create_automotive_gauge_large(lv_obj_t *parent, const char *title, const char *unit, 
                                              lv_color_t color, int16_t x, int16_t y, int32_t min_val, int32_t max_val)
{
    // Container for gauge and labels
    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, GAUGE_SIZE_LARGE + 40, GAUGE_SIZE_LARGE + 80);
    lv_obj_set_pos(container, x, y);
    lv_obj_set_style_bg_color(container, lv_color_hex(UI_COLOR_PANEL), 0);
    lv_obj_set_style_border_width(container, 2, 0);
    lv_obj_set_style_border_color(container, color, 0);
    lv_obj_set_style_radius(container, 15, 0);
    lv_obj_set_style_pad_all(container, 10, 0);
    
    // Title label
    lv_obj_t *title_label = lv_label_create(container);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_color(title_label, lv_color_hex(UI_COLOR_TEXT_WHITE), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 5);
    
    // Arc gauge
    lv_obj_t *arc = lv_arc_create(container);
    lv_obj_set_size(arc, GAUGE_SIZE_LARGE, GAUGE_SIZE_LARGE);
    lv_obj_align(arc, LV_ALIGN_CENTER, 0, 10);
    lv_arc_set_range(arc, min_val, max_val);
    lv_arc_set_value(arc, min_val);
    lv_arc_set_bg_angles(arc, 135, 45);  // 270 degree arc
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    
    // Arc styling
    lv_obj_set_style_arc_width(arc, 12, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(UI_COLOR_BORDER), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 12, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);
    
    // Value label in center (larger font for 7" display)
    lv_obj_t *value_label = lv_label_create(container);
    lv_label_set_text(value_label, "0");
    lv_obj_set_style_text_color(value_label, lv_color_hex(UI_COLOR_TEXT_WHITE), 0);
    lv_obj_set_style_text_font(value_label, LV_FONT_DEFAULT, 0);
    lv_obj_align(value_label, LV_ALIGN_CENTER, 0, 5);
    
    // Unit label
    lv_obj_t *unit_label = lv_label_create(container);
    lv_label_set_text(unit_label, unit);
    lv_obj_set_style_text_color(unit_label, lv_color_hex(UI_COLOR_TEXT_GRAY), 0);
    lv_obj_set_style_text_font(unit_label, &lv_font_montserrat_14, 0);
    lv_obj_align(unit_label, LV_ALIGN_CENTER, 0, 30);
    
    // Min/Max labels
    lv_obj_t *min_label = lv_label_create(container);
    lv_label_set_text_fmt(min_label, "%ld", (long)min_val);
    lv_obj_set_style_text_color(min_label, lv_color_hex(UI_COLOR_TEXT_DARK), 0);
    lv_obj_set_style_text_font(min_label, LV_FONT_DEFAULT, 0);
    lv_obj_align(min_label, LV_ALIGN_BOTTOM_LEFT, 15, -10);
    
    lv_obj_t *max_label = lv_label_create(container);
    lv_label_set_text_fmt(max_label, "%ld", (long)max_val);
    lv_obj_set_style_text_color(max_label, lv_color_hex(UI_COLOR_TEXT_DARK), 0);
    lv_obj_set_style_text_font(max_label, LV_FONT_DEFAULT, 0);
    lv_obj_align(max_label, LV_ALIGN_BOTTOM_RIGHT, -15, -10);
    
    // Store value label reference in arc user data
    lv_obj_set_user_data(arc, value_label);
    
    return arc;
}

/* Update gauge value and arc */
static void update_gauge_value(lv_obj_t *arc, int32_t value, const char *format)
{
    if (!arc) return;
    
    lv_arc_set_value(arc, value);
    
    lv_obj_t *value_label = (lv_obj_t*)lv_obj_get_user_data(arc);
    if (value_label) {
        char buf[16];
        snprintf(buf, sizeof(buf), format, (long)value);
        lv_label_set_text(value_label, buf);
    }
}

/* Set gauge color based on value and thresholds */
static void set_gauge_warning_color(lv_obj_t *arc, int32_t value, int32_t warning_threshold, int32_t critical_threshold)
{
    lv_color_t color;
    
    if (value >= critical_threshold) {
        color = lv_color_hex(UI_COLOR_CRITICAL_RED);
    } else if (value >= warning_threshold) {
        color = lv_color_hex(UI_COLOR_WARNING_RED);
    } else {
        // Get original color from arc style
        color = lv_obj_get_style_arc_color(arc, LV_PART_INDICATOR);
    }
    
    lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);
}

/* Create header panel */
void ui_create_header(lv_obj_t *parent)
{
    header_panel = lv_obj_create(parent);
    lv_obj_set_size(header_panel, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(header_panel, 0, 0);
    lv_obj_set_style_bg_color(header_panel, lv_color_hex(UI_COLOR_PANEL), 0);
    lv_obj_set_style_border_width(header_panel, 0, 0);
    lv_obj_set_style_radius(header_panel, 0, 0);
    lv_obj_set_style_pad_all(header_panel, 10, 0);
    
    // Main title
    label_title = lv_label_create(header_panel);
    lv_label_set_text(label_title, "ECU DASHBOARD - ESP32-S3");
    lv_obj_set_style_text_color(label_title, lv_color_hex(UI_COLOR_TEXT_WHITE), 0);
    lv_obj_set_style_text_font(label_title, &lv_font_montserrat_14, 0);
    lv_obj_align(label_title, LV_ALIGN_LEFT_MID, 0, 0);
    
    // System time/status
    label_time = lv_label_create(header_panel);
    lv_label_set_text(label_time, "00:00:00");
    lv_obj_set_style_text_color(label_time, lv_color_hex(UI_COLOR_ACCENT_BLUE), 0);
    lv_obj_set_style_text_font(label_time, &lv_font_montserrat_14, 0);
    lv_obj_align(label_time, LV_ALIGN_RIGHT_MID, 0, 0);
}

/* Create gauge panel */
void ui_create_gauge_panel(lv_obj_t *parent)
{
    gauge_panel = lv_obj_create(parent);
    lv_obj_set_size(gauge_panel, SCREEN_WIDTH, SCREEN_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT);
    lv_obj_set_pos(gauge_panel, 0, HEADER_HEIGHT);
    lv_obj_set_style_bg_color(gauge_panel, lv_color_hex(UI_COLOR_BACKGROUND), 0);
    lv_obj_set_style_border_width(gauge_panel, 0, 0);
    lv_obj_set_style_radius(gauge_panel, 0, 0);
    lv_obj_set_style_pad_all(gauge_panel, PANEL_PADDING, 0);
    
    // Create gauges in 3x2 grid for 7" display
    int gauge_width = GAUGE_SIZE_LARGE + 40;
    int gauge_height = GAUGE_SIZE_LARGE + 80;
    int start_x = (SCREEN_WIDTH - (3 * gauge_width)) / 4;
    int start_y = 10;
    int spacing_x = gauge_width + start_x;
    int spacing_y = gauge_height + 20;
    
    // Top row
    gauge_rpm = create_automotive_gauge_large(gauge_panel, "ENGINE RPM", "rpm", 
                                             lv_color_hex(UI_COLOR_ACCENT_ORANGE), 
                                             start_x, start_y, 0, 7000);
    
    gauge_map = create_automotive_gauge_large(gauge_panel, "MAP PRESSURE", "kPa", 
                                             lv_color_hex(UI_COLOR_ACCENT_BLUE), 
                                             start_x + spacing_x, start_y, 100, 250);
    
    gauge_tps = create_automotive_gauge_large(gauge_panel, "TPS POSITION", "%", 
                                             lv_color_hex(UI_COLOR_ACCENT_YELLOW), 
                                             start_x + spacing_x * 2, start_y, 0, 100);
    
    // Bottom row (if space allows)
    if (SCREEN_HEIGHT > 400) {
        gauge_wastegate = create_automotive_gauge_large(gauge_panel, "WASTEGATE", "%", 
                                                       lv_color_hex(UI_COLOR_ACCENT_GREEN), 
                                                       start_x, start_y + spacing_y, 0, 100);
        
        gauge_target_boost = create_automotive_gauge_large(gauge_panel, "TARGET BOOST", "kPa", 
                                                          lv_color_hex(UI_COLOR_ACCENT_YELLOW), 
                                                          start_x + spacing_x, start_y + spacing_y, 100, 250);
        
        gauge_coolant_temp = create_automotive_gauge_large(gauge_panel, "COOLANT TEMP", "°C", 
                                                          lv_color_hex(UI_COLOR_ACCENT_BLUE), 
                                                          start_x + spacing_x * 2, start_y + spacing_y, -40, 120);
    }
}

/* Create status panel */
void ui_create_status_panel(lv_obj_t *parent)
{
    // Status panel on the right side if gauges are 2x3, or bottom if 3x2
    status_panel = lv_obj_create(parent);
    lv_obj_set_size(status_panel, 200, 300);
    lv_obj_set_pos(status_panel, SCREEN_WIDTH - 220, HEADER_HEIGHT + 20);
    lv_obj_set_style_bg_color(status_panel, lv_color_hex(UI_COLOR_PANEL), 0);
    lv_obj_set_style_border_width(status_panel, 2, 0);
    lv_obj_set_style_border_color(status_panel, lv_color_hex(UI_COLOR_BORDER), 0);
    lv_obj_set_style_radius(status_panel, 10, 0);
    lv_obj_set_style_pad_all(status_panel, 15, 0);
    
    // Status title
    lv_obj_t *status_title = lv_label_create(status_panel);
    lv_label_set_text(status_title, "SYSTEM STATUS");
    lv_obj_set_style_text_color(status_title, lv_color_hex(UI_COLOR_TEXT_WHITE), 0);
    lv_obj_set_style_text_font(status_title, &lv_font_montserrat_14, 0);
    lv_obj_align(status_title, LV_ALIGN_TOP_MID, 0, 0);
    
    // TCU Status
    lv_obj_t *tcu_title = lv_label_create(status_panel);
    lv_label_set_text(tcu_title, "TCU:");
    lv_obj_set_style_text_color(tcu_title, lv_color_hex(UI_COLOR_TEXT_GRAY), 0);
    lv_obj_set_style_text_font(tcu_title, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(tcu_title, 0, 40);
    
    label_tcu_status = lv_label_create(status_panel);
    lv_label_set_text(label_tcu_status, "NORMAL");
    lv_obj_set_style_text_color(label_tcu_status, lv_color_hex(UI_COLOR_ACCENT_GREEN), 0);
    lv_obj_set_style_text_font(label_tcu_status, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(label_tcu_status, 50, 40);
    
    // Gear Position
    lv_obj_t *gear_title = lv_label_create(status_panel);
    lv_label_set_text(gear_title, "GEAR:");
    lv_obj_set_style_text_color(gear_title, lv_color_hex(UI_COLOR_TEXT_GRAY), 0);
    lv_obj_set_style_text_font(gear_title, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(gear_title, 0, 70);
    
    label_gear = lv_label_create(status_panel);
    lv_label_set_text(label_gear, "N");
    lv_obj_set_style_text_color(label_gear, lv_color_hex(UI_COLOR_TEXT_WHITE), 0);
    lv_obj_set_style_text_font(label_gear, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(label_gear, 70, 70);
    
    // Engine Load
    lv_obj_t *load_title = lv_label_create(status_panel);
    lv_label_set_text(load_title, "LOAD:");
    lv_obj_set_style_text_color(load_title, lv_color_hex(UI_COLOR_TEXT_GRAY), 0);
    lv_obj_set_style_text_font(load_title, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(load_title, 0, 100);
    
    label_engine_load = lv_label_create(status_panel);
    lv_label_set_text(label_engine_load, "0%");
    lv_obj_set_style_text_color(label_engine_load, lv_color_hex(UI_COLOR_TEXT_WHITE), 0);
    lv_obj_set_style_text_font(label_engine_load, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(label_engine_load, 70, 100);
    
    // Wi-Fi Status
    lv_obj_t *wifi_title = lv_label_create(status_panel);
    lv_label_set_text(wifi_title, "Wi-Fi:");
    lv_obj_set_style_text_color(wifi_title, lv_color_hex(UI_COLOR_TEXT_GRAY), 0);
    lv_obj_set_style_text_font(wifi_title, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(wifi_title, 0, 130);
    
    label_wifi_status = lv_label_create(status_panel);
    lv_label_set_text(label_wifi_status, "AP MODE");
    lv_obj_set_style_text_color(label_wifi_status, lv_color_hex(UI_COLOR_ACCENT_YELLOW), 0);
    lv_obj_set_style_text_font(label_wifi_status, LV_FONT_DEFAULT, 0);
    lv_obj_set_pos(label_wifi_status, 0, 150);
}

/* Create footer panel */
void ui_create_footer(lv_obj_t *parent)
{
    footer_panel = lv_obj_create(parent);
    lv_obj_set_size(footer_panel, SCREEN_WIDTH, FOOTER_HEIGHT);
    lv_obj_set_pos(footer_panel, 0, SCREEN_HEIGHT - FOOTER_HEIGHT);
    lv_obj_set_style_bg_color(footer_panel, lv_color_hex(UI_COLOR_PANEL), 0);
    lv_obj_set_style_border_width(footer_panel, 0, 0);
    lv_obj_set_style_radius(footer_panel, 0, 0);
    lv_obj_set_style_pad_all(footer_panel, 10, 0);
    
    // Connection status
    label_connection = lv_label_create(footer_panel);
    lv_label_set_text(label_connection, "CAN: DISCONNECTED");
    lv_obj_set_style_text_color(label_connection, lv_color_hex(UI_COLOR_WARNING_RED), 0);
    lv_obj_set_style_text_font(label_connection, &lv_font_montserrat_14, 0);
    lv_obj_align(label_connection, LV_ALIGN_LEFT_MID, 0, 0);
    
    // ESP32-S3 info with Wi-Fi
    lv_obj_t *label_hw = lv_label_create(footer_panel);
    lv_label_set_text(label_hw, "ESP32-S3-WROOM-1-N16R8 • 7\" Touch LCD • TJA1051 CAN • Wi-Fi");
    lv_obj_set_style_text_color(label_hw, lv_color_hex(UI_COLOR_TEXT_GRAY), 0);
    lv_obj_set_style_text_font(label_hw, LV_FONT_DEFAULT, 0);
    lv_obj_align(label_hw, LV_ALIGN_RIGHT_MID, 0, 0);
}

/* Initialize dashboard UI */
void ui_dashboard_init(void)
{
    ESP_LOGI(TAG, "Initializing ECU Dashboard UI for 7\" Display");
    
    // Set automotive theme
    ui_set_theme_automotive();
    
    // Create main screen
    main_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(main_screen, lv_color_hex(UI_COLOR_BACKGROUND), 0);
    lv_scr_load(main_screen);
    
    // Create UI panels
    ui_create_header(main_screen);
    ui_create_gauge_panel(main_screen);
    ui_create_status_panel(main_screen);
    ui_create_footer(main_screen);
    
    ESP_LOGI(TAG, "ECU Dashboard UI initialized for 800x480 display");
}

/* Update functions */
void ui_update_rpm(uint16_t rpm)
{
    update_gauge_value(gauge_rpm, rpm, "%d");
    set_gauge_warning_color(gauge_rpm, rpm, ECU_WARNING_HIGH_RPM, 7000);
}

void ui_update_map_pressure(uint16_t map_kpa)
{
    update_gauge_value(gauge_map, map_kpa, "%d");
    set_gauge_warning_color(gauge_map, map_kpa, ECU_WARNING_HIGH_MAP, ECU_CRITICAL_HIGH_MAP);
}

void ui_update_tps_position(uint8_t tps_percent)
{
    update_gauge_value(gauge_tps, tps_percent, "%d");
    set_gauge_warning_color(gauge_tps, tps_percent, ECU_WARNING_HIGH_TPS, 100);
}

void ui_update_wastegate_position(uint8_t wg_percent)
{
    if (gauge_wastegate) {
        update_gauge_value(gauge_wastegate, wg_percent, "%d");
    }
}

void ui_update_target_boost(uint16_t target_kpa)
{
    if (gauge_target_boost) {
        update_gauge_value(gauge_target_boost, target_kpa, "%d");
    }
}

void ui_update_tcu_status(bool protection_active, bool limp_mode)
{
    if (limp_mode) {
        lv_label_set_text(label_tcu_status, "LIMP MODE");
        lv_obj_set_style_text_color(label_tcu_status, lv_color_hex(UI_COLOR_CRITICAL_RED), 0);
    } else if (protection_active) {
        lv_label_set_text(label_tcu_status, "PROTECTION");
        lv_obj_set_style_text_color(label_tcu_status, lv_color_hex(UI_COLOR_WARNING_RED), 0);
    } else {
        lv_label_set_text(label_tcu_status, "NORMAL");
        lv_obj_set_style_text_color(label_tcu_status, lv_color_hex(UI_COLOR_ACCENT_GREEN), 0);
    }
}

void ui_update_connection_status(bool connected)
{
    if (connected) {
        lv_label_set_text(label_connection, "CAN: CONNECTED • 500 kbps");
        lv_obj_set_style_text_color(label_connection, lv_color_hex(UI_COLOR_ACCENT_GREEN), 0);
    } else {
        lv_label_set_text(label_connection, "CAN: DISCONNECTED");
        lv_obj_set_style_text_color(label_connection, lv_color_hex(UI_COLOR_WARNING_RED), 0);
    }
}

void ui_update_wifi_status(bool connected, const char *ssid, int8_t rssi)
{
    if (connected && ssid) {
        char wifi_text[128];
        snprintf(wifi_text, sizeof(wifi_text), "%s\n%ld dBm", ssid, (long)rssi);
        lv_label_set_text(label_wifi_status, wifi_text);
        lv_obj_set_style_text_color(label_wifi_status, lv_color_hex(UI_COLOR_ACCENT_GREEN), 0);
    } else {
        lv_label_set_text(label_wifi_status, "AP MODE\nECU_Dashboard_Config");
        lv_obj_set_style_text_color(label_wifi_status, lv_color_hex(UI_COLOR_ACCENT_YELLOW), 0);
    }
}

/* Set automotive theme for large display */
void ui_set_theme_automotive(void)
{
    lv_theme_default_init(NULL, lv_color_hex(UI_COLOR_ACCENT_BLUE), lv_color_hex(UI_COLOR_ACCENT_ORANGE), 
                         LV_THEME_DEFAULT_DARK, LV_FONT_DEFAULT);
}