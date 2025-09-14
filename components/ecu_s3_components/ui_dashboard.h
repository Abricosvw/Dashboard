/*
 * ECU Dashboard UI Components for ESP32-S3 7" Display
 * ESP-IDF Component with LVGL Integration
 */

#ifndef UI_DASHBOARD_H
#define UI_DASHBOARD_H

#include "lvgl.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Color definitions for automotive theme (optimized for 7" display) */
#define UI_COLOR_BACKGROUND     0x0F0F0F
#define UI_COLOR_PANEL          0x1A1A1A
#define UI_COLOR_TEXT_WHITE     0xFFFFFF
#define UI_COLOR_TEXT_GRAY      0x808080
#define UI_COLOR_TEXT_DARK      0x404040
#define UI_COLOR_ACCENT_BLUE    0x00D4FF
#define UI_COLOR_ACCENT_GREEN   0x00FF88
#define UI_COLOR_ACCENT_YELLOW  0xFFD700
#define UI_COLOR_ACCENT_ORANGE  0xFF6B35
#define UI_COLOR_WARNING_RED    0xFF4444
#define UI_COLOR_CRITICAL_RED   0xFF0000
#define UI_COLOR_BORDER         0x333333

/* Gauge configurations for 7" display (800x480) */
#define GAUGE_SIZE_SMALL        100
#define GAUGE_SIZE_MEDIUM       140
#define GAUGE_SIZE_LARGE        180
#define GAUGE_SIZE_XLARGE       220

/* Layout definitions for 7" display */
#define SCREEN_WIDTH            800
#define SCREEN_HEIGHT           480
#define HEADER_HEIGHT           60
#define FOOTER_HEIGHT           40
#define GAUGE_MARGIN            20
#define PANEL_PADDING           10

/* Function prototypes */
void ui_dashboard_init(void);
void ui_update_rpm(uint16_t rpm);
void ui_update_map_pressure(uint16_t map_kpa);
void ui_update_tps_position(uint8_t tps_percent);
void ui_update_wastegate_position(uint8_t wg_percent);
void ui_update_target_boost(uint16_t target_kpa);
void ui_update_tcu_status(bool protection_active, bool limp_mode);
void ui_update_connection_status(bool connected);
void ui_update_wifi_status(bool connected, const char *ssid, int8_t rssi);
void ui_set_theme_automotive(void);

/* Layout management */
void ui_create_header(lv_obj_t *parent);
void ui_create_gauge_panel(lv_obj_t *parent);
void ui_create_status_panel(lv_obj_t *parent);
void ui_create_footer(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif

#endif /* UI_DASHBOARD_H */