// ECU Dashboard Screen 5 - ECU Data Gauges (Page 2)
#include "../ui.h"
#include "ui_Screen5.h"
#include "ui_screen_manager.h"
#include "ui_helpers.h"
#include <stdio.h>
#include <esp_log.h>

// Screen object
lv_obj_t * ui_Screen5;

// Gauge Objects
lv_obj_t * ui_Arc_Eng_TQ_Act;
lv_obj_t * ui_Label_Eng_TQ_Act_Value;
lv_obj_t * ui_Arc_Limit_TQ;
lv_obj_t * ui_Label_Limit_TQ_Value;

// Animation variables
static lv_anim_t anim_eng_tq_act;
static lv_anim_t anim_limit_tq;

// Function prototypes
static void swipe_handler_screen5(lv_event_t * e);
static void anim_value_cb_screen5(void * var, int32_t v);

// Helper function to create a gauge
static void create_gauge(lv_obj_t * parent, lv_obj_t ** arc, lv_obj_t ** label,
                        const char * title, const char * unit, lv_color_t color,
                        int32_t min_val, int32_t max_val, int x, int y)
{
    lv_obj_t * cont = lv_obj_create(parent);
    lv_obj_set_width(cont, 250);
    lv_obj_set_height(cont, 200);
    lv_obj_set_x(cont, x);
    lv_obj_set_y(cont, y);
    lv_obj_set_align(cont, LV_ALIGN_TOP_LEFT);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_color(cont, color, 0);
    lv_obj_set_style_border_width(cont, 2, 0);
    lv_obj_set_style_radius(cont, 15, 0);
    lv_obj_set_style_pad_all(cont, 10, 0);

    lv_obj_t * label_title = lv_label_create(cont);
    lv_label_set_text(label_title, title);
    lv_obj_set_style_text_color(label_title, lv_color_white(), 0);
    lv_obj_align(label_title, LV_ALIGN_BOTTOM_MID, 0, -15);

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

    *label = lv_label_create(cont);
    lv_label_set_text(*label, "0");
    lv_obj_set_style_text_color(*label, lv_color_white(), 0);
    lv_obj_center(*label);
    lv_obj_align(*label, LV_ALIGN_CENTER, 0, -5);

    lv_obj_t * label_unit = lv_label_create(cont);
    lv_label_set_text(label_unit, unit);
    lv_obj_set_style_text_color(label_unit, lv_color_hex(0xcccccc), 0);
    lv_obj_align_to(label_unit, *label, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
}


// Main screen initialization
void ui_Screen5_screen_init(void) {
    ui_Screen5 = lv_obj_create(NULL);
    lv_obj_set_size(ui_Screen5, 800, 480);
    lv_obj_set_pos(ui_Screen5, 0, 0);
    lv_obj_clear_flag(ui_Screen5, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen5, lv_color_hex(0x1a1a1a), 0);

    // Title removed as per user request to provide more space.

    // Centered vertically
    create_gauge(ui_Screen5, &ui_Arc_Eng_TQ_Act, &ui_Label_Eng_TQ_Act_Value, "Eng Tq Act", "Nm", lv_color_hex(0x00D4FF), 0, 500, 15, 140);
    create_gauge(ui_Screen5, &ui_Arc_Limit_TQ, &ui_Label_Limit_TQ_Value, "Torque Limit", "Nm", lv_color_hex(0x00FF88), 0, 500, 285, 140);

    // Add standardized navigation buttons
    ui_create_standard_navigation_buttons(ui_Screen5);

    lv_obj_add_event_cb(ui_Screen5, swipe_handler_screen5, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(ui_Screen5, swipe_handler_screen5, LV_EVENT_RELEASED, NULL);

    // Initialize animations
    lv_anim_init(&anim_eng_tq_act);
    lv_anim_set_var(&anim_eng_tq_act, ui_Arc_Eng_TQ_Act);
    lv_anim_set_values(&anim_eng_tq_act, 0, 500);
    lv_anim_set_time(&anim_eng_tq_act, 3000);
    lv_anim_set_playback_time(&anim_eng_tq_act, 3000);
    lv_anim_set_repeat_count(&anim_eng_tq_act, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&anim_eng_tq_act, anim_value_cb_screen5);

    lv_anim_init(&anim_limit_tq);
    lv_anim_set_var(&anim_limit_tq, ui_Arc_Limit_TQ);
    lv_anim_set_values(&anim_limit_tq, 0, 500);
    lv_anim_set_time(&anim_limit_tq, 4000);
    lv_anim_set_playback_time(&anim_limit_tq, 4000);
    lv_anim_set_repeat_count(&anim_limit_tq, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&anim_limit_tq, anim_value_cb_screen5);

    if (demo_mode_get_enabled()) {
        ui_Screen5_update_animations(true);
    }

    ESP_LOGI("SCREEN5", "Screen 5 initialized");
}

static void anim_value_cb_screen5(void * var, int32_t v)
{
    lv_arc_set_value((lv_obj_t *)var, v);

    if (var == ui_Arc_Eng_TQ_Act) lv_label_set_text_fmt(ui_Label_Eng_TQ_Act_Value, "%d", v);
    else if (var == ui_Arc_Limit_TQ) lv_label_set_text_fmt(ui_Label_Limit_TQ_Value, "%d", v);
}

void ui_Screen5_update_animations(bool demo_enabled)
{
    if (demo_enabled) {
        lv_anim_start(&anim_eng_tq_act);
        lv_anim_start(&anim_limit_tq);
    } else {
        lv_anim_del(ui_Arc_Eng_TQ_Act, anim_value_cb_screen5);
        lv_anim_del(ui_Arc_Limit_TQ, anim_value_cb_screen5);
    }
}

static void swipe_handler_screen5(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_RELEASED) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        if (dir == LV_DIR_LEFT) ui_switch_to_next_enabled_screen(true);
        if (dir == LV_DIR_RIGHT) ui_switch_to_next_enabled_screen(false);
    }
}


void ui_Screen5_screen_destroy(void) {
    if (ui_Screen5) {
        lv_obj_del(ui_Screen5);
        ui_Screen5 = NULL;
    }
}
