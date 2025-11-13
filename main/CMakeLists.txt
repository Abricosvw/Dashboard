set(SRCS
    "main.c"
    "background_task.c"
    "can_parser.c"
    "can_websocket.c"
    "canbus.c"
    "cmd_i2ctools.c"
    "ecu_data.c"
    "mre_parser.c"
    "sd_card.c"
    "serial_reader.c"
    "web_server.c"
    "wifi_server.c"
)

file(GLOB_RECURSE UI_SRCS "ui/*.c")

list(APPEND SRCS ${UI_SRCS})

idf_component_register(SRCS ${SRCS}
                      INCLUDE_DIRS "." "include" "ui"
                      PRIV_REQUIRES lvgl esp_lcd_touch_gt911 console nvs_flash esp_http_server esp_wifi fatfs sdmmc)
