// ECU Dashboard Screen 3 - Advanced CAN Bus Terminal
// Third screen dedicated to CAN Bus monitoring with advanced features

#include "../ui.h"
#include "ui_Screen3.h"
#include "ui_Screen2.h"
#include "ui_Screen1.h"
#include "ui_screen_manager.h"
#include "ui_helpers.h"
#include "ui_events.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// Screen object
lv_obj_t * ui_Screen3;

// CAN Bus Terminal Objects
void * ui_TextArea_CAN_Terminal;
void * ui_Label_CAN_Status;
void * ui_Label_CAN_Count;

// Advanced CAN Terminal Objects
void * ui_Button_Clear;
void * ui_Button_Sniffer;
void * ui_TextArea_Search;
void * ui_Slider_UpdateSpeed;
void * ui_Label_UpdateSpeed;

// CAN Terminal state
static int can_message_count = 0;
static int can_sniffer_active = 1;   // Sniffer mode active
static char search_text[64] = "";
static int update_speed_ms = 100;    // Default update speed

// Sniffer state
static int last_can_id = 0;
static uint8_t last_can_data[8] = {0};
static uint8_t last_can_dlc = 0;

// Function prototypes
static void screen3_touch_handler(lv_event_t * e);
static void swipe_handler_screen3(lv_event_t * e);
static void clear_button_event_cb(lv_event_t * e);
static void sniffer_button_event_cb(lv_event_t * e);
static void search_text_event_cb(lv_event_t * e);
static void update_speed_slider_event_cb(lv_event_t * e);
static int is_message_matches_search(const char* message);

// CAN Sniffer functions
static void can_sniffer_add_message(uint32_t id, uint8_t *data, uint8_t dlc);
static void can_sniffer_format_message(char *buffer, size_t size, uint32_t id, uint8_t *data, uint8_t dlc);
static int can_sniffer_is_id_filtered(uint32_t id);
static int can_sniffer_search_in_data(uint8_t *data, uint8_t dlc, const char *search_term);
static void can_sniffer_update_statistics(uint32_t id);

// Swipe handler for screen switching
static void swipe_handler_screen3(lv_event_t * e) {
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
            
            // Swipe right to go to previous enabled screen (backward direction)
            if (delta_x > 50) {
                // Switch to previous enabled screen
                ui_switch_to_next_enabled_screen(false);
            }
            // Swipe left to go to next enabled screen (forward direction)
            else if (delta_x < -50) {
                // Switch to next enabled screen
                ui_switch_to_next_enabled_screen(true);
            }
        }
        is_swiping = 0;
    }
}

// Clear button event callback
static void clear_button_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ui_clear_can_terminal();
    }
}

// Sniffer button event callback
static void sniffer_button_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t * btn = (lv_obj_t*)e->target;

        // Toggle sniffer state
        can_sniffer_active = !can_sniffer_active;
        ui_set_can_sniffer_active(can_sniffer_active);

        // Update button appearance
        if (can_sniffer_active) {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x00FF88), 0); // Green when active
            lv_obj_t * label = lv_obj_get_child(btn, 0);
            if (label) {
                lv_label_set_text(label, "SNIFFER: ON");
            }
        } else {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0xFF3366), 0); // Red when inactive
            lv_obj_t * label = lv_obj_get_child(btn, 0);
            if (label) {
                lv_label_set_text(label, "SNIFFER: OFF");
            }
        }
    }
}



// Search text event callback
static void search_text_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        const char* text = lv_textarea_get_text((lv_obj_t*)ui_TextArea_Search);
        if (text) {
            strncpy(search_text, text, sizeof(search_text) - 1);
            search_text[sizeof(search_text) - 1] = '\0';
        }
    }
}

// Update speed slider event callback
static void update_speed_slider_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        int32_t value = lv_slider_get_value((lv_obj_t*)ui_Slider_UpdateSpeed);
        update_speed_ms = (int)value;
        
        // Update speed label
        char speed_text[32];
        snprintf(speed_text, sizeof(speed_text), "Update Speed: %dms", update_speed_ms);
        lv_label_set_text((lv_obj_t*)ui_Label_UpdateSpeed, speed_text);
    }
}



// Check if message matches search text
static int is_message_matches_search(const char* message) {
    if (strlen(search_text) == 0) return 1; // No search text, show all
    
    // Case-insensitive search
    char message_lower[256];
    char search_lower[64];
    strncpy(message_lower, message, sizeof(message_lower) - 1);
    strncpy(search_lower, search_text, sizeof(search_lower) - 1);
    
    // Convert to lowercase
    for (int i = 0; message_lower[i]; i++) {
        if (message_lower[i] >= 'A' && message_lower[i] <= 'Z') {
            message_lower[i] = message_lower[i] + 32;
        }
    }
    for (int i = 0; search_lower[i]; i++) {
        if (search_lower[i] >= 'A' && search_lower[i] <= 'Z') {
            search_lower[i] = search_lower[i] + 32;
        }
    }
    
    return strstr(message_lower, search_lower) != NULL;
}





// Initialize Screen3
void ui_Screen3_screen_init(void)
{
    ui_Screen3 = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Screen3, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen3, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(ui_Screen3, LV_OPA_COVER, 0);
    
    // Main terminal container (LEFT SIDE)
    lv_obj_t * terminal_cont = lv_obj_create(ui_Screen3);
    lv_obj_set_width(terminal_cont, 450);
    lv_obj_set_height(terminal_cont, 410);
    lv_obj_set_x(terminal_cont, 10);
    lv_obj_set_y(terminal_cont, 60);
    lv_obj_set_align(terminal_cont, LV_ALIGN_TOP_LEFT);
    lv_obj_clear_flag(terminal_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(terminal_cont, lv_color_hex(0x00000000), 0);
    lv_obj_set_style_border_width(terminal_cont, 0, 0);
    lv_obj_set_style_pad_all(terminal_cont, 0, 0);
    
    // Terminal title (stays in place)
    lv_obj_t * terminal_title = lv_label_create(ui_Screen3);
    lv_label_set_text(terminal_title, "Advanced CAN Bus Terminal");
    lv_obj_set_style_text_color(terminal_title, lv_color_hex(0x00D4FF), 0);
    lv_obj_set_style_text_font(terminal_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_bg_color(terminal_title, lv_color_hex(0x2a2a2a), 0);
    lv_obj_align(terminal_title, LV_ALIGN_TOP_MID, 0, 10);
    
    // CAN Terminal text area (main display) - LEFT SIDE
    ui_TextArea_CAN_Terminal = lv_textarea_create(terminal_cont);
    lv_obj_set_size((lv_obj_t*)ui_TextArea_CAN_Terminal, 450, 410);
    lv_obj_set_pos((lv_obj_t*)ui_TextArea_CAN_Terminal, 0, 0);
    lv_obj_set_style_bg_color((lv_obj_t*)ui_TextArea_CAN_Terminal, lv_color_hex(0x00000000), 0);
    lv_obj_set_style_text_color((lv_obj_t*)ui_TextArea_CAN_Terminal, lv_color_hex(0x00FF88), 0);
    lv_obj_set_style_border_color((lv_obj_t*)ui_TextArea_CAN_Terminal, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width((lv_obj_t*)ui_TextArea_CAN_Terminal, 1, 0);
    lv_obj_set_style_radius((lv_obj_t*)ui_TextArea_CAN_Terminal, 5, 0);
    lv_obj_set_style_text_font((lv_obj_t*)ui_TextArea_CAN_Terminal, &lv_font_montserrat_12, 0);
    lv_textarea_set_placeholder_text((lv_obj_t*)ui_TextArea_CAN_Terminal, "Waiting for CAN Bus data...");
    lv_textarea_set_text((lv_obj_t*)ui_TextArea_CAN_Terminal, "TIME         | ID  | DLC | DATA                     | ASCII    | DBC COMMENTS\n");
    lv_obj_clear_flag((lv_obj_t*)ui_TextArea_CAN_Terminal, LV_OBJ_FLAG_CLICKABLE);
    
    // RIGHT SIDE PANEL - Controls and Status
    lv_obj_t * right_panel = lv_obj_create(ui_Screen3);
    lv_obj_set_width(right_panel, 320);
    lv_obj_set_height(right_panel, 410);
    lv_obj_align(right_panel, LV_ALIGN_TOP_RIGHT, -10, 60);
    lv_obj_clear_flag(right_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(right_panel, lv_color_hex(0x00000000), 0);
    lv_obj_set_style_border_width(right_panel, 0, 0);
    lv_obj_set_style_pad_all(right_panel, 5, 0);
    lv_obj_set_flex_flow(right_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(right_panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(right_panel, 8, 0);

    // --- Top row: Status, Count, and Clear button ---
    lv_obj_t* top_row = lv_obj_create(right_panel);
    lv_obj_remove_style_all(top_row); // Remove default styles
    lv_obj_set_width(top_row, LV_PCT(100));
    lv_obj_set_height(top_row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(top_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // CAN Status indicator
    ui_Label_CAN_Status = lv_label_create(top_row);
    lv_label_set_text((lv_obj_t*)ui_Label_CAN_Status, "● CAN: DISCONNECTED");
    lv_obj_set_style_text_color((lv_obj_t*)ui_Label_CAN_Status, lv_color_hex(0xFF3366), 0);
    lv_obj_set_style_text_font((lv_obj_t*)ui_Label_CAN_Status, &lv_font_montserrat_10, 0);
    
    // CAN Message counter
    ui_Label_CAN_Count = lv_label_create(top_row);
    lv_label_set_text((lv_obj_t*)ui_Label_CAN_Count, "Messages: 0");
    lv_obj_set_style_text_color((lv_obj_t*)ui_Label_CAN_Count, lv_color_hex(0x00FF88), 0);
    lv_obj_set_style_text_font((lv_obj_t*)ui_Label_CAN_Count, &lv_font_montserrat_14, 0);

    // Clear button
    ui_Button_Clear = lv_btn_create(top_row);
    lv_obj_set_size((lv_obj_t*)ui_Button_Clear, 80, 30);
    lv_obj_set_style_bg_color((lv_obj_t*)ui_Button_Clear, lv_color_hex(0xFF3366), 0);
    lv_obj_set_style_radius((lv_obj_t*)ui_Button_Clear, 15, 0);
    lv_obj_add_event_cb((lv_obj_t*)ui_Button_Clear, clear_button_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * clear_label = lv_label_create((lv_obj_t*)ui_Button_Clear);
    lv_label_set_text(clear_label, "CLEAR");
    lv_obj_set_style_text_color(clear_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(clear_label, &lv_font_montserrat_12, 0);
    lv_obj_center(clear_label);

    // --- Control buttons row ---
    lv_obj_t * control_cont = lv_obj_create(right_panel);
    lv_obj_remove_style_all(control_cont);
    lv_obj_set_width(control_cont, LV_PCT(100));
    lv_obj_set_height(control_cont, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(control_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(control_cont, 5, 5);
    lv_obj_set_flex_align(control_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Sniffer button
    ui_Button_Sniffer = lv_btn_create(control_cont);
    lv_obj_set_size((lv_obj_t*)ui_Button_Sniffer, 120, 30);
    lv_obj_set_style_bg_color((lv_obj_t*)ui_Button_Sniffer, lv_color_hex(0x00FF88), 0); // Start with green (active)
    lv_obj_set_style_radius((lv_obj_t*)ui_Button_Sniffer, 15, 0);
    lv_obj_add_event_cb((lv_obj_t*)ui_Button_Sniffer, sniffer_button_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * sniffer_label = lv_label_create((lv_obj_t*)ui_Button_Sniffer);
    lv_label_set_text(sniffer_label, "SNIFFER: ON");
    lv_obj_set_style_text_color(sniffer_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(sniffer_label, &lv_font_montserrat_10, 0);
    lv_obj_center(sniffer_label);

    // --- Search row ---
    lv_obj_t * search_cont = lv_obj_create(right_panel);
    lv_obj_remove_style_all(search_cont);
    lv_obj_set_width(search_cont, LV_PCT(100));
    lv_obj_set_height(search_cont, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(search_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(search_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(search_cont, 10, 0);
    lv_obj_set_style_bg_color(search_cont, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_pad_all(search_cont, 5, 0);
    lv_obj_set_style_radius(search_cont, 8, 0);

    lv_obj_t * search_label = lv_label_create(search_cont);
    lv_label_set_text(search_label, "Search:");
    lv_obj_set_style_text_color(search_label, lv_color_hex(0x00D4FF), 0);
    lv_obj_set_style_text_font(search_label, &lv_font_montserrat_14, 0);
    
    ui_TextArea_Search = lv_textarea_create(search_cont);
    lv_obj_set_flex_grow((lv_obj_t*)ui_TextArea_Search, 1);
    lv_obj_set_height((lv_obj_t*)ui_TextArea_Search, 35);
    lv_textarea_set_one_line((lv_obj_t*)ui_TextArea_Search, true);
    lv_obj_set_style_bg_color((lv_obj_t*)ui_TextArea_Search, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_color((lv_obj_t*)ui_TextArea_Search, lv_color_white(), 0);
    lv_obj_set_style_border_width((lv_obj_t*)ui_TextArea_Search, 0, 0);
    lv_obj_set_style_radius((lv_obj_t*)ui_TextArea_Search, 5, 0);
    lv_obj_set_style_text_font((lv_obj_t*)ui_TextArea_Search, &lv_font_montserrat_12, 0);
    lv_textarea_set_placeholder_text((lv_obj_t*)ui_TextArea_Search, "Enter search text...");
    lv_obj_add_event_cb((lv_obj_t*)ui_TextArea_Search, search_text_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // --- Update speed slider ---
    lv_obj_t * slider_cont = lv_obj_create(right_panel);
    lv_obj_remove_style_all(slider_cont);
    lv_obj_set_width(slider_cont, LV_PCT(100));
    lv_obj_set_height(slider_cont, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(slider_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(slider_cont, 5, 0);
    lv_obj_set_style_bg_color(slider_cont, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_pad_all(slider_cont, 5, 0);
    lv_obj_set_style_radius(slider_cont, 8, 0);

    ui_Label_UpdateSpeed = lv_label_create(slider_cont);
    char speed_text[32];
    snprintf(speed_text, sizeof(speed_text), "Update Speed: %dms", update_speed_ms);
    lv_label_set_text((lv_obj_t*)ui_Label_UpdateSpeed, speed_text);
    lv_obj_set_style_text_color((lv_obj_t*)ui_Label_UpdateSpeed, lv_color_hex(0x00D4FF), 0);
    lv_obj_set_style_text_font((lv_obj_t*)ui_Label_UpdateSpeed, &lv_font_montserrat_12, 0);
    lv_obj_align((lv_obj_t*)ui_Label_UpdateSpeed, LV_ALIGN_LEFT_MID, 0, 0);

    ui_Slider_UpdateSpeed = lv_slider_create(slider_cont);
    lv_obj_set_width((lv_obj_t*)ui_Slider_UpdateSpeed, LV_PCT(100));
    lv_slider_set_range((lv_obj_t*)ui_Slider_UpdateSpeed, 50, 1000);
    lv_slider_set_value((lv_obj_t*)ui_Slider_UpdateSpeed, update_speed_ms, LV_ANIM_OFF);
    lv_obj_set_style_bg_color((lv_obj_t*)ui_Slider_UpdateSpeed, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa((lv_obj_t*)ui_Slider_UpdateSpeed, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color((lv_obj_t*)ui_Slider_UpdateSpeed, lv_color_hex(0x00D4FF), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color((lv_obj_t*)ui_Slider_UpdateSpeed, lv_color_hex(0x00D4FF), LV_PART_KNOB);
    lv_obj_add_event_cb((lv_obj_t*)ui_Slider_UpdateSpeed, update_speed_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Add swipe functionality for screen switching
    lv_obj_add_event_cb(ui_Screen3, swipe_handler_screen3, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(ui_Screen3, swipe_handler_screen3, LV_EVENT_RELEASED, NULL);
    
    // Add standardized navigation buttons
    ui_create_standard_navigation_buttons(ui_Screen3);
}

// Destroy Screen3
void ui_Screen3_screen_destroy(void)
{
    lv_obj_del(ui_Screen3);
}

// Add CAN message to terminal with search
void ui_add_can_message(const char* message)
{
    if (ui_TextArea_CAN_Terminal) {
        // Check search text
        if (!is_message_matches_search(message)) {
            return; // Message doesn't match search
        }
        
        // Update message count
        can_message_count++;
        
        // Statistics removed - no longer tracking message counts
        
        // Update message count display
        char count_text[32];
        snprintf(count_text, sizeof(count_text), "Messages: %d", can_message_count);
        lv_label_set_text((lv_obj_t*)ui_Label_CAN_Count, count_text);
        
        // Get current text and add new message
        const char* current_text = lv_textarea_get_text((lv_obj_t*)ui_TextArea_CAN_Terminal);
        char new_text[4096]; // Increased buffer size
        
        // Find the header line to insert the new message after it
        const char* header = strstr(current_text, "\n");
        if (header) {
            int header_len = (header - current_text) + 1;
            // Copy header
            strncpy(new_text, current_text, header_len);
            new_text[header_len] = '\0';
            // Append new message
            strncat(new_text, message, sizeof(new_text) - strlen(new_text) - 1);
            strncat(new_text, "\n", sizeof(new_text) - strlen(new_text) - 1);
            // Append the rest of the old messages
            strncat(new_text, header + 1, sizeof(new_text) - strlen(new_text) - 1);
        } else {
            // Fallback if header not found (should not happen after init)
            snprintf(new_text, sizeof(new_text), "%s\n%s", message, current_text);
        }
        
        // Limit text length to prevent memory issues
        if (strlen(new_text) > 4000) {
            // Keep header and last ~3000 chars
            char* first_line_end = strstr(new_text, "\n");
            if (first_line_end) {
                char header_buf[100];
                int header_len = first_line_end - new_text + 1;
                strncpy(header_buf, new_text, header_len);
                header_buf[header_len] = '\0';

                char* truncated_body = new_text + strlen(new_text) - 3000;
                char* first_msg_in_trunc = strstr(truncated_body, "\n");
                if(first_msg_in_trunc) truncated_body = first_msg_in_trunc + 1;

                snprintf(new_text, sizeof(new_text), "%s%s", header_buf, truncated_body);
            }
        }
        
        lv_textarea_set_text((lv_obj_t*)ui_TextArea_CAN_Terminal, new_text);
    }
}

// Update CAN status and message count
void ui_update_can_status(int connected, int message_count)
{
    if (ui_Label_CAN_Status) {
        if (connected) {
            lv_label_set_text((lv_obj_t*)ui_Label_CAN_Status, "● CAN: CONNECTED");
            lv_obj_set_style_text_color((lv_obj_t*)ui_Label_CAN_Status, lv_color_hex(0x00FF88), 0);
        } else {
            lv_label_set_text((lv_obj_t*)ui_Label_CAN_Status, "● CAN: DISCONNECTED");
            lv_obj_set_style_text_color((lv_obj_t*)ui_Label_CAN_Status, lv_color_hex(0xFF3366), 0);
        }
    }
    
    if (ui_Label_CAN_Count) {
        char count_text[32];
        snprintf(count_text, sizeof(count_text), "Messages: %d", message_count);
        lv_label_set_text((lv_obj_t*)ui_Label_CAN_Count, count_text);
    }
}

// Update touch cursor position for Screen3
void ui_update_touch_cursor_screen3(void * point)
{
    if (ui_Touch_Cursor_Screen3 && point) {
        lv_point_t * p = (lv_point_t*)point;
        lv_obj_set_pos((lv_obj_t*)ui_Touch_Cursor_Screen3, p->x - 15, p->y - 15);
        lv_obj_clear_flag((lv_obj_t*)ui_Touch_Cursor_Screen3, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_opa((lv_obj_t*)ui_Touch_Cursor_Screen3, 255, 0);
    }
}

// Clear CAN terminal
void ui_clear_can_terminal(void)
{
    if (ui_TextArea_CAN_Terminal) {
        lv_textarea_set_text((lv_obj_t*)ui_TextArea_CAN_Terminal, "TIME         | ID  | DLC | DATA                     | ASCII    | DBC COMMENTS\n");
        can_message_count = 0;
        lv_label_set_text((lv_obj_t*)ui_Label_CAN_Count, "Messages: 0");
    }
}



// Set search text
void ui_set_search_text(const char* search_text_input)
{
    if (search_text_input) {
        strncpy(search_text, search_text_input, sizeof(search_text) - 1);
        search_text[sizeof(search_text) - 1] = '\0';
        
        if (ui_TextArea_Search) {
            lv_textarea_set_text((lv_obj_t*)ui_TextArea_Search, search_text);
        }
    }
}

// Set update speed
void ui_set_update_speed(int speed_ms)
{
    update_speed_ms = speed_ms;
    if (ui_Slider_UpdateSpeed) {
        lv_slider_set_value((lv_obj_t*)ui_Slider_UpdateSpeed, speed_ms, LV_ANIM_OFF);
    }
    if (ui_Label_UpdateSpeed) {
        char speed_text[32];
        snprintf(speed_text, sizeof(speed_text), "Update Speed: %dms", update_speed_ms);
        lv_label_set_text((lv_obj_t*)ui_Label_UpdateSpeed, speed_text);
    }
}

// Update CAN statistics display (statistics no longer displayed)
void ui_update_can_statistics(void)
{
    // Statistics display removed - function kept for compatibility
}

// Reset CAN statistics (statistics no longer displayed)
void ui_reset_can_statistics(void)
{
    // Statistics display removed - function kept for compatibility
}

// ============================================================================
// CAN SNIFFER FUNCTIONS
// ============================================================================

// Add CAN message to terminal with filtering and search
static void can_sniffer_add_message(uint32_t id, uint8_t *data, uint8_t dlc)
{
    if (!can_sniffer_active) return;

    // Check if ID should be filtered
    if (!can_sniffer_is_id_filtered(id)) return;

    // Check if search term matches
    if (!can_sniffer_search_in_data(data, dlc, search_text)) return;

    // Format message
    char message_buffer[256];
    can_sniffer_format_message(message_buffer, sizeof(message_buffer), id, data, dlc);

    // Add to terminal
    ui_add_can_message(message_buffer);

    // Update statistics
    can_sniffer_update_statistics(id);

    // Store last message for debugging
    last_can_id = id;
    last_can_dlc = dlc;
    memcpy(last_can_data, data, dlc > 8 ? 8 : dlc);
}

// Format CAN message for display
static void can_sniffer_format_message(char *buffer, size_t size, uint32_t id, uint8_t *data, uint8_t dlc)
{
    // Timestamp
    char timestamp_str[16];
    uint32_t tick = lv_tick_get();
    snprintf(timestamp_str, sizeof(timestamp_str), "%lu.%03lu", tick / 1000, (tick % 1000));

    // HEX Data
    char data_hex_str[3 * 8 + 1] = {0};
    for (int i = 0; i < dlc && i < 8; i++) {
        char byte_str[4];
        snprintf(byte_str, sizeof(byte_str), "%02X ", data[i]);
        strncat(data_hex_str, byte_str, sizeof(data_hex_str) - strlen(data_hex_str) - 1);
    }

    // ASCII Data
    char data_ascii_str[9] = {0};
    for (int i = 0; i < dlc && i < 8; i++) {
        if (data[i] >= 32 && data[i] <= 126) { // Printable ASCII
            data_ascii_str[i] = data[i];
        } else {
            data_ascii_str[i] = '.'; // Non-printable
        }
    }
    data_ascii_str[dlc] = '\0';

    // DBC Comments (placeholder)
    const char* dbc_comment = ""; // Placeholder for future DBC implementation

    // Final formatted string
    snprintf(buffer, size, "%-12s | %-3X | %-1d | %-24s | %-8s | %s",
             timestamp_str,
             (unsigned int)id,
             dlc,
             data_hex_str,
             data_ascii_str,
             dbc_comment);
}

// Check if CAN ID should be shown (always show all messages now)
static int can_sniffer_is_id_filtered(uint32_t id)
{
    return 1; // Show all messages
}

// Search for text in CAN data (supports hex values and ASCII)
static int can_sniffer_search_in_data(uint8_t *data, uint8_t dlc, const char *search_term)
{
    if (!search_term || strlen(search_term) == 0) return 1; // No search term, show all

    char search_lower[64];
    strncpy(search_lower, search_term, sizeof(search_lower) - 1);

    // Convert search term to lowercase
    for (int i = 0; search_lower[i]; i++) {
        if (search_lower[i] >= 'A' && search_lower[i] <= 'Z') {
            search_lower[i] = search_lower[i] + 32;
        }
    }

    // Search for hex values (e.g., "AA", "FF")
    if (strlen(search_lower) == 2) {
        uint8_t search_byte;
        if (sscanf(search_lower, "%2hhx", &search_byte) == 1) {
            for (int i = 0; i < dlc && i < 8; i++) {
                if (data[i] == search_byte) return 1;
            }
        }
    }

    // Search for longer hex sequences (e.g., "AABB", "FF0011")
    if (strlen(search_lower) > 2 && strlen(search_lower) % 2 == 0) {
        uint8_t search_bytes[8];
        int num_bytes = strlen(search_lower) / 2;
        if (num_bytes <= 8) {
            for (int i = 0; i < num_bytes; i++) {
                if (sscanf(&search_lower[i * 2], "%2hhx", &search_bytes[i]) != 1) {
                    break; // Invalid hex, try text search
                }
            }
            // Check if data contains this sequence
            for (int i = 0; i <= dlc - num_bytes; i++) {
                if (memcmp(&data[i], search_bytes, num_bytes) == 0) return 1;
            }
        }
    }

    // Search for ASCII text in data (simple approach)
    char data_ascii[9] = "";
    for (int i = 0; i < dlc && i < 8; i++) {
        if (data[i] >= 32 && data[i] <= 126) { // Printable ASCII
            data_ascii[i] = data[i];
        } else {
            data_ascii[i] = '.'; // Non-printable
        }
    }

    // Convert to lowercase for comparison
    for (int i = 0; data_ascii[i]; i++) {
        if (data_ascii[i] >= 'A' && data_ascii[i] <= 'Z') {
            data_ascii[i] = data_ascii[i] + 32;
        }
    }

    return strstr(data_ascii, search_lower) != NULL;
}

// Update statistics counters (statistics removed)
static void can_sniffer_update_statistics(uint32_t id)
{
    // Statistics display removed - function simplified
}

// Enable/disable CAN sniffer
void ui_set_can_sniffer_active(int active)
{
    can_sniffer_active = active;
    if (active) {
        // Clear terminal when enabling
        ui_clear_can_terminal();
    }
}

// Get last CAN message for debugging
void ui_get_last_can_message(uint32_t *id, uint8_t *data, uint8_t *dlc)
{
    if (id) *id = last_can_id;
    if (data) memcpy(data, last_can_data, 8);
    if (dlc) *dlc = last_can_dlc;
}

// Process real CAN message from ESP32 TWAI driver
void ui_process_real_can_message(uint32_t id, uint8_t *data, uint8_t dlc)
{
    if (can_sniffer_active) {
        can_sniffer_add_message(id, data, dlc);
    }
}
