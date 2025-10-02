/*
 * Wi-Fi Manager for ESP32-S3-WROOM-1-N16R8
 * ESP-IDF Component for wireless connectivity and OTA updates
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_http_server.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Wi-Fi Configuration */
#define WIFI_SSID_MAX_LEN           32
#define WIFI_PASSWORD_MAX_LEN       64
#define WIFI_MAXIMUM_RETRY          5
#define WIFI_CONNECT_TIMEOUT_MS     30000

/* Access Point Configuration */
#define WIFI_AP_SSID                "ECU_Dashboard_Config"
#define WIFI_AP_PASSWORD            "ecu123456"
#define WIFI_AP_CHANNEL             1
#define WIFI_AP_MAX_CONNECTIONS     4

/* HTTP Server Configuration */
#define HTTP_SERVER_PORT            8080
#define WEBSOCKET_SERVER_PORT       8080

/* Wi-Fi Status */
typedef enum {
    WIFI_STATUS_DISCONNECTED = 0,
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_AP_STARTED,
    WIFI_STATUS_FAILED
} wifi_status_t;

/* Wi-Fi Events */
typedef enum {
    WIFI_MGR_EVENT_CONNECTED = 0,
    WIFI_MGR_EVENT_DISCONNECTED,
    WIFI_MGR_EVENT_GOT_IP,
    WIFI_MGR_EVENT_SCAN_DONE,
    WIFI_MGR_EVENT_AP_STARTED,
    WIFI_MGR_EVENT_AP_STOPPED
} wifi_mgr_event_type_t;

/* Wi-Fi Event Callback */
typedef void (*wifi_mgr_event_callback_t)(wifi_mgr_event_type_t event, void *data);

/* Wi-Fi Configuration Structure */
typedef struct {
    char ssid[WIFI_SSID_MAX_LEN];
    char password[WIFI_PASSWORD_MAX_LEN];
    bool auto_connect;
    bool enable_ap_fallback;
    wifi_mgr_event_callback_t event_callback;
} wifi_mgr_config_t;

/* Scan Result Structure */
typedef struct {
    char ssid[33];
    int8_t rssi;
    wifi_auth_mode_t authmode;
    bool is_open;
} wifi_scan_result_t;

/* Function Prototypes */
esp_err_t wifi_manager_init(wifi_mgr_config_t *config);
esp_err_t wifi_manager_deinit(void);
esp_err_t wifi_manager_connect(const char *ssid, const char *password);
esp_err_t wifi_manager_disconnect(void);
esp_err_t wifi_manager_start_ap(void);
esp_err_t wifi_manager_stop_ap(void);
esp_err_t wifi_manager_scan_networks(wifi_scan_result_t *results, size_t *result_count);
wifi_status_t wifi_manager_get_status(void);
esp_err_t wifi_manager_get_ip_info(esp_netif_ip_info_t *ip_info);
bool wifi_manager_is_connected(void);
int8_t wifi_manager_get_rssi(void);
const char* wifi_manager_get_ssid(void);

/* HTTP Server Functions */
esp_err_t wifi_http_server_start(void);
esp_err_t wifi_http_server_stop(void);
bool wifi_http_server_is_running(void);

/* OTA Update Functions */
esp_err_t wifi_ota_start_update(const char *url);
bool wifi_ota_is_updating(void);
int wifi_ota_get_progress(void);

/* Data Transmission Functions */
esp_err_t wifi_send_ecu_data(const char *json_data);

/* Remote ECU Data Functions */
esp_err_t wifi_start_ecu_server(void);
esp_err_t wifi_send_ecu_data(const char *json_data);
esp_err_t wifi_send_ecu_broadcast(const char *json_data);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_MANAGER_H */