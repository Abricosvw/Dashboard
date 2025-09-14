#ifndef UI_SCREEN1_H
#define UI_SCREEN1_H

#ifdef __cplusplus
extern "C" {
#endif

// SCREEN: ui_Screen1
extern void ui_Screen1_screen_init(void);
extern void ui_Screen1_screen_destroy(void);
extern lv_obj_t * ui_Screen1;
extern lv_obj_t * ui_Arc_MAP;
extern lv_obj_t * ui_Arc_Wastegate;
extern lv_obj_t * ui_Arc_TPS;
extern lv_obj_t * ui_Arc_RPM;
extern lv_obj_t * ui_Arc_Boost;
extern lv_obj_t * ui_LED_TCU;

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif