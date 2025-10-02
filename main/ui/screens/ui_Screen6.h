// ECU Dashboard Screen 6 - Device Parameters Settings
// Sixth screen dedicated to device configuration and settings

#ifndef UI_SCREEN6_H
#define UI_SCREEN6_H

#ifdef __cplusplus
extern "C" {
#endif

// SCREEN: ui_Screen6
extern void ui_Screen6_screen_init(void);
extern void ui_Screen6_screen_destroy(void);
extern lv_obj_t * ui_Screen6;

// Device Parameters Settings Objects
extern void * ui_Label_Device_Title;
extern void * ui_Button_Demo_Mode;
extern void * ui_Button_Enable_Screen3;
extern void * ui_Button_Save_Settings;
extern void * ui_Button_Reset_Settings;

// Touch cursor object
extern lv_obj_t * ui_Touch_Cursor_Screen6;

// Functions for device parameters settings
extern void ui_update_device_settings_display(void);
extern void ui_save_device_settings(void);
extern void ui_reset_device_settings(void);

// Screen6 utility functions
extern void ui_Screen6_load_settings(void);
extern void ui_Screen6_save_settings(void);
extern void ui_Screen6_update_button_states(void);

// Touch cursor update function for Screen6
extern void ui_update_touch_cursor_screen6(void * point);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
