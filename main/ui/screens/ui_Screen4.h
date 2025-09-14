// ECU Dashboard Screen 4 - MRE Data Gauges (Page 1)
#ifndef UI_SCREEN4_H
#define UI_SCREEN4_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

void ui_Screen4_screen_init(void);
void ui_Screen4_screen_destroy(void);
void ui_Screen4_update_animations(bool demo_enabled);
extern lv_obj_t * ui_Screen4;
extern lv_obj_t * ui_Arc_Abs_Pedal;
extern lv_obj_t * ui_Arc_WG_Pos;
extern lv_obj_t * ui_Arc_BOV;
extern lv_obj_t * ui_Arc_TCU_TQ_Req;
extern lv_obj_t * ui_Arc_TCU_TQ_Act;
extern lv_obj_t * ui_Arc_Eng_TQ_Req;
extern lv_obj_t * ui_Label_Abs_Pedal_Value;
extern lv_obj_t * ui_Label_WG_Pos_Value;
extern lv_obj_t * ui_Label_BOV_Value;
extern lv_obj_t * ui_Label_TCU_TQ_Req_Value;
extern lv_obj_t * ui_Label_TCU_TQ_Act_Value;
extern lv_obj_t * ui_Label_Eng_TQ_Req_Value;

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
