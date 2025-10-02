// ECU Dashboard Screen with 6 animated gauges
// Based on test project structure

#include "../ui.h"

lv_obj_t * ui_Screen1 = NULL;
lv_obj_t * ui_Arc_MAP = NULL;
lv_obj_t * ui_Arc_Wastegate = NULL;
lv_obj_t * ui_Arc_TPS = NULL;
lv_obj_t * ui_Arc_RPM = NULL;
lv_obj_t * ui_Arc_Boost = NULL;
lv_obj_t * ui_LED_TCU = NULL;

lv_obj_t * ui_Label_MAP_Value = NULL;
lv_obj_t * ui_Label_Wastegate_Value = NULL;
lv_obj_t * ui_Label_TPS_Value = NULL;
lv_obj_t * ui_Label_RPM_Value = NULL;
lv_obj_t * ui_Label_Boost_Value = NULL;
lv_obj_t * ui_Label_TCU_Status = NULL;

static lv_anim_t anim_map;
static lv_anim_t anim_wastegate;
static lv_anim_t anim_tps;
static lv_anim_t anim_rpm;
static lv_anim_t anim_boost;

static void anim_value_cb(void * var, int32_t v)
{
    lv_arc_set_value((lv_obj_t *)var, v);
    
    if(var == ui_Arc_MAP) {
        lv_label_set_text_fmt(ui_Label_MAP_Value, "%d", v);
    }
    else if(var == ui_Arc_Wastegate) {
        lv_label_set_text_fmt(ui_Label_Wastegate_Value, "%d", v);
    }
    else if(var == ui_Arc_TPS) {
        lv_label_set_text_fmt(ui_Label_TPS_Value, "%d", v);
    }
    else if(var == ui_Arc_RPM) {
        lv_label_set_text_fmt(ui_Label_RPM_Value, "%d", v);
        
        // Update TCU status based on RPM
        if(v > 5500) {
            lv_led_set_color(ui_LED_TCU, lv_color_hex(0xFF0000));
            lv_label_set_text(ui_Label_TCU_Status, "ERROR");
            lv_obj_set_style_text_color(ui_Label_TCU_Status, lv_color_hex(0xFF0000), 0);
        }
        else if(v > 4500) {
            lv_led_set_color(ui_LED_TCU, lv_color_hex(0xFFAA00));
            lv_label_set_text(ui_Label_TCU_Status, "WARNING");
            lv_obj_set_style_text_color(ui_Label_TCU_Status, lv_color_hex(0xFFAA00), 0);
        }
        else {
            lv_led_set_color(ui_LED_TCU, lv_color_hex(0x00FF00));
            lv_label_set_text(ui_Label_TCU_Status, "OK");
            lv_obj_set_style_text_color(ui_Label_TCU_Status, lv_color_hex(0x00FF00), 0);
        }
    }
    else if(var == ui_Arc_Boost) {
        lv_label_set_text_fmt(ui_Label_Boost_Value, "%d", v);
    }
}

static void create_gauge(lv_obj_t * parent, lv_obj_t ** arc, lv_obj_t ** label,
                        const char * title, const char * unit, lv_color_t color,
                        int32_t min_val, int32_t max_val, int x, int y)
{
    // Container
    lv_obj_t * cont = lv_obj_create(parent);
    lv_obj_set_width(cont, 250);
    lv_obj_set_height(cont, 230);
    lv_obj_set_x(cont, x);
    lv_obj_set_y(cont, y);
    lv_obj_set_align(cont, LV_ALIGN_TOP_LEFT);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_color(cont, color, 0);
    lv_obj_set_style_border_width(cont, 2, 0);
    lv_obj_set_style_radius(cont, 15, 0);
    lv_obj_set_style_pad_all(cont, 10, 0);
    
    // Title
    lv_obj_t * label_title = lv_label_create(cont);
    lv_label_set_text(label_title, title);
    lv_obj_set_style_text_color(label_title, lv_color_white(), 0);
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 10);
    
    // Arc
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
    
    // Value label
    *label = lv_label_create(cont);
    lv_label_set_text(*label, "0");
    lv_obj_set_style_text_color(*label, lv_color_white(), 0);
    lv_obj_center(*label);
    lv_obj_align(*label, LV_ALIGN_CENTER, 0, -5);
    
    // Unit label
    lv_obj_t * label_unit = lv_label_create(cont);
    lv_label_set_text(label_unit, unit);
    lv_obj_set_style_text_color(label_unit, lv_color_hex(0xcccccc), 0);
    lv_obj_align_to(label_unit, *label, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
}

void ui_Screen1_screen_init(void)
{
    ui_Screen1 = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Screen1, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen1, lv_color_hex(0x1a1a1a), 0);
    
    // Create gauges in 3x2 grid
    create_gauge(ui_Screen1, &ui_Arc_MAP, &ui_Label_MAP_Value, 
                "MAP Pressure", "kPa", lv_color_hex(0x00D4FF), 100, 250, 15, 15);
    
    create_gauge(ui_Screen1, &ui_Arc_Wastegate, &ui_Label_Wastegate_Value,
                "Wastegate", "%", lv_color_hex(0x00FF88), 0, 100, 275, 15);
    
    create_gauge(ui_Screen1, &ui_Arc_TPS, &ui_Label_TPS_Value,
                "TPS Position", "%", lv_color_hex(0xFFD700), 0, 100, 535, 15);
    
    create_gauge(ui_Screen1, &ui_Arc_RPM, &ui_Label_RPM_Value,
                "Engine RPM", "RPM", lv_color_hex(0xFF6B35), 0, 7000, 15, 255);
    
    create_gauge(ui_Screen1, &ui_Arc_Boost, &ui_Label_Boost_Value,
                "Target Boost", "kPa", lv_color_hex(0xFFD700), 100, 250, 275, 255);
    
    // TCU Status indicator
    lv_obj_t * tcu_cont = lv_obj_create(ui_Screen1);
    lv_obj_set_width(tcu_cont, 250);
    lv_obj_set_height(tcu_cont, 230);
    lv_obj_set_x(tcu_cont, 535);
    lv_obj_set_y(tcu_cont, 255);
    lv_obj_set_align(tcu_cont, LV_ALIGN_TOP_LEFT);
    lv_obj_clear_flag(tcu_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(tcu_cont, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_color(tcu_cont, lv_color_hex(0x4a4a4a), 0);
    lv_obj_set_style_border_width(tcu_cont, 2, 0);
    lv_obj_set_style_radius(tcu_cont, 15, 0);
    
    lv_obj_t * tcu_title = lv_label_create(tcu_cont);
    lv_label_set_text(tcu_title, "TCU Status");
    lv_obj_set_style_text_color(tcu_title, lv_color_white(), 0);
    lv_obj_align(tcu_title, LV_ALIGN_TOP_MID, 0, 20);
    
    ui_LED_TCU = lv_led_create(tcu_cont);
    lv_obj_set_size(ui_LED_TCU, 80, 80);
    lv_obj_center(ui_LED_TCU);
    lv_led_set_color(ui_LED_TCU, lv_color_hex(0x00FF00));
    lv_led_on(ui_LED_TCU);
    
    ui_Label_TCU_Status = lv_label_create(tcu_cont);
    lv_label_set_text(ui_Label_TCU_Status, "OK");
    lv_obj_set_style_text_color(ui_Label_TCU_Status, lv_color_hex(0x00FF00), 0);
    lv_obj_align(ui_Label_TCU_Status, LV_ALIGN_BOTTOM_MID, 0, -20);
    
    // Setup animations
    lv_anim_init(&anim_map);
    lv_anim_set_var(&anim_map, ui_Arc_MAP);
    lv_anim_set_values(&anim_map, 100, 250);
    lv_anim_set_time(&anim_map, 3000);
    lv_anim_set_playback_time(&anim_map, 3000);
    lv_anim_set_repeat_count(&anim_map, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&anim_map, anim_value_cb);
    lv_anim_start(&anim_map);
    
    lv_anim_init(&anim_wastegate);
    lv_anim_set_var(&anim_wastegate, ui_Arc_Wastegate);
    lv_anim_set_values(&anim_wastegate, 0, 100);
    lv_anim_set_time(&anim_wastegate, 2500);
    lv_anim_set_playback_time(&anim_wastegate, 2500);
    lv_anim_set_repeat_count(&anim_wastegate, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&anim_wastegate, anim_value_cb);
    lv_anim_start(&anim_wastegate);
    
    lv_anim_init(&anim_tps);
    lv_anim_set_var(&anim_tps, ui_Arc_TPS);
    lv_anim_set_values(&anim_tps, 0, 100);
    lv_anim_set_time(&anim_tps, 2000);
    lv_anim_set_playback_time(&anim_tps, 2000);
    lv_anim_set_repeat_count(&anim_tps, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&anim_tps, anim_value_cb);
    lv_anim_start(&anim_tps);
    
    lv_anim_init(&anim_rpm);
    lv_anim_set_var(&anim_rpm, ui_Arc_RPM);
    lv_anim_set_values(&anim_rpm, 800, 6500);
    lv_anim_set_time(&anim_rpm, 4000);
    lv_anim_set_playback_time(&anim_rpm, 4000);
    lv_anim_set_repeat_count(&anim_rpm, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&anim_rpm, anim_value_cb);
    lv_anim_start(&anim_rpm);
    
    lv_anim_init(&anim_boost);
    lv_anim_set_var(&anim_boost, ui_Arc_Boost);
    lv_anim_set_values(&anim_boost, 100, 250);
    lv_anim_set_time(&anim_boost, 3500);
    lv_anim_set_playback_time(&anim_boost, 3500);
    lv_anim_set_repeat_count(&anim_boost, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&anim_boost, anim_value_cb);
    lv_anim_start(&anim_boost);
}

void ui_Screen1_screen_destroy(void)
{
    if(ui_Screen1) lv_obj_del(ui_Screen1);
    ui_Screen1 = NULL;
}