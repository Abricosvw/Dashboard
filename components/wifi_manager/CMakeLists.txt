idf_component_register(SRCS "wifi_manager.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi esp_http_server esp_https_ota nvs_flash lwip freertos log esp_netif esp_event esp_common esp_system app_update)