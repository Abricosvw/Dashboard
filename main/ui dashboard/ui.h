#ifndef _ECU_DASHBOARD_UI_H
#define _ECU_DASHBOARD_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "ui_helpers.h"
#include "ui_events.h"

///////////////////// SCREENS ////////////////////
#include "screens/ui_Screen1.h"

///////////////////// VARIABLES ////////////////////

// EVENTS
extern lv_obj_t * ui____initial_actions0;

// UI INIT
void ui_init(void);
void ui_destroy(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif