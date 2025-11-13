# Add sources from ui directory
file(GLOB_RECURSE SRC_UI ${CMAKE_CURRENT_SOURCE_DIR} "ui/*.c")

idf_component_register(
    SRCS "main.c" ${SRC_UI}
    INCLUDE_DIRS "." "ui"
    REQUIRES esp_lcd lvgl driver esp_lcd_touch_gt911
)