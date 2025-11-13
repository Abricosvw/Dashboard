/*
 * ECU Dashboard UI Components
 * Animated gauges for ESP32-S3-Touch-LCD-7
 */

#include "lvgl.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "LVGL_UI";

/* Gauge objects */
static lv_obj_t *arc_map;
static lv_obj_t *arc_wastegate;
static lv_obj_t *arc_tps;
static lv_obj_t *arc_rpm;
static lv_obj_t *arc_boost;
static lv_obj_t *led_tcu;

static lv_obj_t *label_map_value;
static lv_obj_t *label_wastegate_value;
static lv_obj_t *label_tps_value;
static lv_obj_t *label_rpm_value;
static lv_obj_t *label_boost_value;
static lv_obj_t *label_tcu_status;

/* Animation values */
static lv_anim_t anim_map;
static lv_anim_t anim_wastegate;
static lv_anim_t anim_tps;
static lv_anim_t anim_rpm;
static lv_anim_t anim_boost;

/* Forward declarations */
static void anim_set_value(void *obj, int32_t value);
static void create_gauge(lv_obj_t *parent, lv_obj_t **arc, lv_obj_t **label_value, 
                        const char *title, const char *unit, lv_color_t color,
                        int32_t min_val, int32_t max_val, int x, int y);

void example_lvgl_demo_ui(lv_disp_t *disp)
{
    lv_obj_t *scr = lv_disp_get_scr_act(disp);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a1a), 0);
    
    ESP_LOGI(TAG, "Creating ECU Dashboard with 6 gauges");
    
    /* Create gauges in 3x2 grid */
    create_gauge(scr, &arc_map, &label_map_value, "MAP Pressure", "kPa", 
                 lv_color_hex(0x00D4FF), 100, 250, 0, 0);
    
    create_gauge(scr, &arc_wastegate, &label_wastegate_value, "Wastegate", "%", 
                 lv_color_hex(0x00FF88), 0, 100, 267, 0);
    
    create_gauge(scr, &arc_tps, &label_tps_value, "TPS Position", "%", 
                 lv_color_hex(0xFFD700), 0, 100, 534, 0);
    
    create_gauge(scr, &arc_rpm, &label_rpm_value, "Engine RPM", "RPM", 
                 lv_color_hex(0xFF6B35), 0, 7000, 0, 240);
    
    create_gauge(scr, &arc_boost, &label_boost_value, "Target Boost", "kPa", 
                 lv_color_hex(0xFFD700), 100, 250, 267, 240);
    
    /* TCU Status indicator */
    lv_obj_t *tcu_cont = lv_obj_create(scr);
    lv_obj_set_size(tcu_cont, 250, 230);
    lv_obj_set_pos(tcu_cont, 534, 240);
    lv_obj_set_style_bg_color(tcu_cont, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_color(tcu_cont, lv_color_hex(0x4a4a4a), 0);
    lv_obj_set_style_border_width(tcu_cont, 2, 0);
    lv_obj_set_style_radius(tcu_cont, 15, 0);
    
    lv_obj_t *tcu_title = lv_label_create(tcu_cont);
    lv_label_set_text(tcu_title, "TCU Status");
    lv_obj_set_style_text_color(tcu_title, lv_color_white(), 0);
    lv_obj_align(tcu_title, LV_ALIGN_TOP_MID, 0, 10);
    
    led_tcu = lv_led_create(tcu_cont);
    lv_obj_set_size(led_tcu, 80, 80);
    lv_obj_center(led_tcu);
    lv_led_set_color(led_tcu, lv_color_hex(0x00FF00));
    lv_led_on(led_tcu);
    
    label_tcu_status = lv_label_create(tcu_cont);
    lv_label_set_text(label_tcu_status, "OK");
    lv_obj_set_style_text_color(label_tcu_status, lv_color_hex(0x00FF00), 0);
    lv_obj_align(label_tcu_status, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    /* Setup animations */
    ESP_LOGI(TAG, "Starting gauge animations");
    
    /* MAP animation */
    lv_anim_init(&anim_map);
    lv_anim_set_var(&anim_map, arc_map);
    lv_anim_set_values(&anim_map, 100, 250);
    lv_anim_set_time(&anim_map, 3000);
    lv_anim_set_playback_time(&anim_map, 3000);
    lv_anim_set_repeat_count(&anim_map, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&anim_map, anim_set_value);
    lv_anim_start(&anim_map);
    
    /* Wastegate animation */
    lv_anim_init(&anim_wastegate);
    lv_anim_set_var(&anim_wastegate, arc_wastegate);
    lv_anim_set_values(&anim_wastegate, 0, 100);
    lv_anim_set_time(&anim_wastegate, 2500);
    lv_anim_set_playback_time(&anim_wastegate, 2500);
    lv_anim_set_repeat_count(&anim_wastegate, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&anim_wastegate, anim_set_value);
    lv_anim_start(&anim_wastegate);
    
    /* TPS animation */
    lv_anim_init(&anim_tps);
    lv_anim_set_var(&anim_tps, arc_tps);
    lv_anim_set_values(&anim_tps, 0, 100);
    lv_anim_set_time(&anim_tps, 2000);
    lv_anim_set_playback_time(&anim_tps, 2000);
    lv_anim_set_repeat_count(&anim_tps, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&anim_tps, anim_set_value);
    lv_anim_start(&anim_tps);
    
    /* RPM animation */
    lv_anim_init(&anim_rpm);
    lv_anim_set_var(&anim_rpm, arc_rpm);
    lv_anim_set_values(&anim_rpm, 800, 6500);
    lv_anim_set_time(&anim_rpm, 4000);
    lv_anim_set_playback_time(&anim_rpm, 4000);
    lv_anim_set_repeat_count(&anim_rpm, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&anim_rpm, anim_set_value);
    lv_anim_start(&anim_rpm);
    
    /* Boost animation */
    lv_anim_init(&anim_boost);
    lv_anim_set_var(&anim_boost, arc_boost);
    lv_anim_set_values(&anim_boost, 100, 250);
    lv_anim_set_time(&anim_boost, 3500);
    lv_anim_set_playback_time(&anim_boost, 3500);
    lv_anim_set_repeat_count(&anim_boost, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&anim_boost, anim_set_value);
    lv_anim_start(&anim_boost);
}

static void create_gauge(lv_obj_t *parent, lv_obj_t **arc, lv_obj_t **label_value, 
                        const char *title, const char *unit, lv_color_t color,
                        int32_t min_val, int32_t max_val, int x, int y)
{
    /* Container */
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 250, 230);
    lv_obj_set_pos(cont, x, y);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_color(cont, color, 0);
    lv_obj_set_style_border_width(cont, 2, 0);
    lv_obj_set_style_radius(cont, 15, 0);
    lv_obj_set_style_pad_all(cont, 10, 0);
    
    /* Title */
    lv_obj_t *label_title = lv_label_create(cont);
    lv_label_set_text(label_title, title);
    lv_obj_set_style_text_color(label_title, lv_color_white(), 0);
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 10);
    
    /* Arc */
    *arc = lv_arc_create(cont);
    lv_obj_set_size(*arc, 160, 160);
    lv_arc_set_rotation(*arc, 135);
    lv_arc_set_bg_angles(*arc, 0, 270);
    lv_arc_set_range(*arc, min_val, max_val);
    lv_arc_set_value(*arc, min_val);
    lv_obj_set_style_arc_color(*arc, color, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(*arc, 15, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(*arc, lv_color_hex(0x4a4a4a), LV_PART_MAIN);
    lv_obj_set_style_arc_width(*arc, 15, LV_PART_MAIN);
    lv_obj_center(*arc);
    lv_obj_remove_style(*arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(*arc, LV_OBJ_FLAG_CLICKABLE);
    
    /* Value label */
    *label_value = lv_label_create(cont);
    lv_label_set_text(*label_value, "0");
    lv_obj_set_style_text_color(*label_value, lv_color_white(), 0);
    lv_obj_center(*label_value);
    lv_obj_align(*label_value, LV_ALIGN_CENTER, 0, -5);
    
    /* Unit label */
    lv_obj_t *label_unit = lv_label_create(cont);
    lv_label_set_text(label_unit, unit);
    lv_obj_set_style_text_color(label_unit, lv_color_hex(0xcccccc), 0);
    lv_obj_align_to(label_unit, *label_value, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    
    /* Store label reference in arc user data */
    lv_obj_set_user_data(*arc, *label_value);
}

static void anim_set_value(void *obj, int32_t value)
{
    lv_obj_t *arc = (lv_obj_t *)obj;
    lv_arc_set_value(arc, value);
    
    /* Update value label */
    lv_obj_t *label = (lv_obj_t *)lv_obj_get_user_data(arc);
    if (label) {
        lv_label_set_text_fmt(label, "%d", value);
    }
    
    /* Special handling for RPM gauge - update TCU status */
    if (arc == arc_rpm) {
        if (value > 5500) {
            lv_led_set_color(led_tcu, lv_color_hex(0xFF0000));
            lv_label_set_text(label_tcu_status, "ERROR");
            lv_obj_set_style_text_color(label_tcu_status, lv_color_hex(0xFF0000), 0);
        } else if (value > 4500) {
            lv_led_set_color(led_tcu, lv_color_hex(0xFFAA00));
            lv_label_set_text(label_tcu_status, "WARNING");
            lv_obj_set_style_text_color(label_tcu_status, lv_color_hex(0xFFAA00), 0);
        } else {
            lv_led_set_color(led_tcu, lv_color_hex(0x00FF00));
            lv_label_set_text(label_tcu_status, "OK");
            lv_obj_set_style_text_color(label_tcu_status, lv_color_hex(0x00FF00), 0);
        }
    }
}