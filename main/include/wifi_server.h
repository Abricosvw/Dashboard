#ifndef WIFI_SERVER_H
#define WIFI_SERVER_H

#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_wifi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// WiFi configuration - using ESP-IDF wifi_config_t
// Custom configuration wrapper
typedef struct {
    char ssid[32];
    char password[64];  // Not used in open network mode
    bool ap_mode;  // true for AP mode, false for STA mode
    char ip_address[16];
} custom_wifi_config_t;

// WebSocket client structure - removed for now

// Function prototypes
esp_err_t wifi_server_init(void);
esp_err_t wifi_server_start(void);
esp_err_t wifi_server_stop(void);
void wifi_server_broadcast_ecu_data(void);
void wifi_server_set_config(const wifi_config_t *config);
wifi_config_t* wifi_server_get_config(void);
bool wifi_is_connected(void);
int wifi_get_client_count(void);

// WebSocket functions - removed for now

// HTTP endpoints
esp_err_t handle_api_ecu_data(httpd_req_t *req);
esp_err_t handle_api_datastream(httpd_req_t *req);
esp_err_t handle_root(httpd_req_t *req);
esp_err_t handle_options(httpd_req_t *req);

// Static file handlers
esp_err_t handle_static_files(httpd_req_t *req);
esp_err_t handle_js_files(httpd_req_t *req);
esp_err_t handle_css_files(httpd_req_t *req);

#ifdef __cplusplus
}
#endif

#endif // WIFI_SERVER_H
