idf_component_register(SRCS "ui_dashboard.c"
                    INCLUDE_DIRS "."
                    REQUIRES freertos log lvgl esp_common esp_system)