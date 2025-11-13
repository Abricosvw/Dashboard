/*
 * Wi-Fi Manager Implementation for ESP32-S3-WROOM-1-N16R8
 * ESP-IDF Component for wireless connectivity and ECU data sharing
 */

#include "wifi_manager.h"
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_http_server.h>
#include <esp_app_format.h>
#include <esp_https_ota.h>
#include <nvs_flash.h>
#include <lwip/sockets.h>
#include <esp_mac.h>

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static const char* TAG = "WIFI_MANAGER";

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* Event bits */
#define WIFI_CONNECTED_BIT      BIT0
#define WIFI_FAIL_BIT          BIT1
#define WIFI_SCAN_DONE_BIT     BIT2

/* Global variables */
static esp_netif_t *sta_netif = NULL;
static esp_netif_t *ap_netif = NULL;
static httpd_handle_t http_server = NULL;
static wifi_mgr_config_t wifi_cfg;
static wifi_status_t current_status = WIFI_STATUS_DISCONNECTED;
static int retry_count = 0;


/* HTTP Server handlers */
static esp_err_t root_get_handler(httpd_req_t *req);
static esp_err_t wifi_config_get_handler(httpd_req_t *req);
static esp_err_t wifi_config_post_handler(httpd_req_t *req);
static esp_err_t wifi_scan_get_handler(httpd_req_t *req);

/* Wi-Fi event handler */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        current_status = WIFI_STATUS_CONNECTING;
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (retry_count < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            retry_count++;
            ESP_LOGI(TAG, "Retrying to connect to the AP (attempt %d/%d)", retry_count, WIFI_MAXIMUM_RETRY);
            current_status = WIFI_STATUS_CONNECTING;
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGI(TAG,"Connect to the AP failed");
            current_status = WIFI_STATUS_FAILED;
            
            // Start AP mode as fallback if enabled
            if (wifi_cfg.enable_ap_fallback) {
                wifi_manager_start_ap();
            }
        }
        
        if (wifi_cfg.event_callback) {
            wifi_cfg.event_callback(WIFI_MGR_EVENT_DISCONNECTED, NULL);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        retry_count = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        current_status = WIFI_STATUS_CONNECTED;
        
        if (wifi_cfg.event_callback) {
            wifi_cfg.event_callback(WIFI_MGR_EVENT_GOT_IP, &event->ip_info);
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Station %02x:%02x:%02x:%02x:%02x:%02x joined, AID=%d",
                 event->mac[0], event->mac[1], event->mac[2], 
                 event->mac[3], event->mac[4], event->mac[5], event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Station %02x:%02x:%02x:%02x:%02x:%02x left, AID=%d",
                 event->mac[0], event->mac[1], event->mac[2], 
                 event->mac[3], event->mac[4], event->mac[5], event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        ESP_LOGI(TAG, "Wi-Fi scan completed");
        xEventGroupSetBits(s_wifi_event_group, WIFI_SCAN_DONE_BIT);
        
        if (wifi_cfg.event_callback) {
            wifi_cfg.event_callback(WIFI_MGR_EVENT_SCAN_DONE, NULL);
        }
    }
}

/* Initialize Wi-Fi Manager */
esp_err_t wifi_manager_init(wifi_mgr_config_t *config)
{
    if (!config) return ESP_ERR_INVALID_ARG;
    
    ESP_LOGI(TAG, "Initializing Wi-Fi Manager for ESP32-S3-WROOM-1...");
    
    memcpy(&wifi_cfg, config, sizeof(wifi_mgr_config_t));
    
    s_wifi_event_group = xEventGroupCreate();
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    sta_netif = esp_netif_create_default_wifi_sta();
    ap_netif = esp_netif_create_default_wifi_ap();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));
    
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "Wi-Fi Manager initialized");
    
    // Auto-connect if configured
    if (config->auto_connect && strlen(config->ssid) > 0) {
        return wifi_manager_connect(config->ssid, config->password);
    }
    
    return ESP_OK;
}

/* Connect to Wi-Fi network */
esp_err_t wifi_manager_connect(const char *ssid, const char *password)
{
    if (!ssid) return ESP_ERR_INVALID_ARG;
    
    ESP_LOGI(TAG, "Connecting to Wi-Fi: %s", ssid);
    
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (password) {
        strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    }
    
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_connect());
    
    // Save configuration
    strncpy(wifi_cfg.ssid, ssid, sizeof(wifi_cfg.ssid) - 1);
    if (password) {
        strncpy(wifi_cfg.password, password, sizeof(wifi_cfg.password) - 1);
    }
    
    current_status = WIFI_STATUS_CONNECTING;
    retry_count = 0;
    
    return ESP_OK;
}

/* Start Access Point mode */
esp_err_t wifi_manager_start_ap(void)
{
    ESP_LOGI(TAG, "Starting Access Point: %s", WIFI_AP_SSID);
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    
    wifi_config_t wifi_config = {
        .ap = {
            .ssid_len = strlen(WIFI_AP_SSID),
            .channel = WIFI_AP_CHANNEL,
            .password = WIFI_AP_PASSWORD,
            .max_connection = WIFI_AP_MAX_CONNECTIONS,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    
    strcpy((char*)wifi_config.ap.ssid, WIFI_AP_SSID);
    
    if (strlen(WIFI_AP_PASSWORD) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    current_status = WIFI_STATUS_AP_STARTED;
    
    // Start HTTP configuration server
    wifi_http_server_start();
    
    ESP_LOGI(TAG, "Access Point started. SSID: %s, Password: %s", WIFI_AP_SSID, WIFI_AP_PASSWORD);
    
    if (wifi_cfg.event_callback) {
        wifi_cfg.event_callback(WIFI_MGR_EVENT_AP_STARTED, NULL);
    }
    
    return ESP_OK;
}

/* Scan for Wi-Fi networks */
esp_err_t wifi_manager_scan_networks(wifi_scan_result_t *results, size_t *result_count)
{
    if (!results || !result_count) return ESP_ERR_INVALID_ARG;
    
    ESP_LOGI(TAG, "Starting Wi-Fi scan...");
    
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 120,
        .scan_time.active.max = 150,
    };
    
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, false));
    
    // Wait for scan to complete
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                          WIFI_SCAN_DONE_BIT,
                                          pdTRUE,
                                          pdFALSE,
                                          pdMS_TO_TICKS(10000));
    
    if (!(bits & WIFI_SCAN_DONE_BIT)) {
        ESP_LOGW(TAG, "Wi-Fi scan timeout");
        return ESP_ERR_TIMEOUT;
    }
    
    uint16_t ap_count = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    
    if (ap_count == 0) {
        *result_count = 0;
        return ESP_OK;
    }
    
    wifi_ap_record_t *ap_info = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (!ap_info) {
        return ESP_ERR_NO_MEM;
    }
    
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_info));
    
    size_t copy_count = (*result_count < ap_count) ? *result_count : ap_count;
    
    for (size_t i = 0; i < copy_count; i++) {
        strncpy(results[i].ssid, (char*)ap_info[i].ssid, sizeof(results[i].ssid) - 1);
        results[i].ssid[sizeof(results[i].ssid) - 1] = '\0';
        results[i].rssi = ap_info[i].rssi;
        results[i].authmode = ap_info[i].authmode;
        results[i].is_open = (ap_info[i].authmode == WIFI_AUTH_OPEN);
    }
    
    *result_count = copy_count;
    free(ap_info);
    
    ESP_LOGI(TAG, "Found %d Wi-Fi networks", ap_count);
    return ESP_OK;
}

/* Get Wi-Fi status */
wifi_status_t wifi_manager_get_status(void)
{
    return current_status;
}

/* Check if connected */
bool wifi_manager_is_connected(void)
{
    return (current_status == WIFI_STATUS_CONNECTED);
}

/* Get IP information */
esp_err_t wifi_manager_get_ip_info(esp_netif_ip_info_t *ip_info)
{
    if (!ip_info || !sta_netif) return ESP_ERR_INVALID_ARG;
    
    return esp_netif_get_ip_info(sta_netif, ip_info);
}

/* Get RSSI */
int8_t wifi_manager_get_rssi(void)
{
    wifi_ap_record_t ap_info;
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
    if (ret != ESP_OK) {
        return -100; // No signal
    }
    return ap_info.rssi;
}

/* Get current SSID */
const char* wifi_manager_get_ssid(void)
{
    return wifi_cfg.ssid;
}

/* HTTP Server Implementation */
static esp_err_t root_get_handler(httpd_req_t *req)
{
    const char* html_page = 
        "<!DOCTYPE html><html><head><title>ECU Dashboard Config</title>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<style>body{font-family:Arial;margin:40px;background:#f0f0f0;}"
        ".container{max-width:600px;margin:auto;background:white;padding:20px;border-radius:8px;}"
        "input,select,button{width:100%;padding:10px;margin:10px 0;border:1px solid #ddd;border-radius:4px;}"
        "button{background:#007bff;color:white;border:none;cursor:pointer;}"
        "button:hover{background:#0056b3;}</style></head><body>"
        "<div class='container'><h1>ECU Dashboard Configuration</h1>"
        "<h2>Wi-Fi Settings</h2><form action='/wifi' method='post'>"
        "<input type='text' name='ssid' placeholder='Wi-Fi SSID' required>"
        "<input type='password' name='password' placeholder='Wi-Fi Password'>"
        "<button type='submit'>Connect</button></form>"
        "<h2>Available Networks</h2><div id='networks'>Loading...</div>"
        "<h2>Current Status</h2><div id='status'>Loading...</div>"
        "<script>setInterval(()=>{fetch('/status').then(r=>r.json()).then(d=>{"
        "document.getElementById('status').innerHTML=`Status: ${d.status}<br>IP: ${d.ip}<br>RSSI: ${d.rssi} dBm`;})},2000);"
        "fetch('/scan').then(r=>r.json()).then(d=>{"
        "document.getElementById('networks').innerHTML=d.networks.map(n=>"
        "`<div>${n.ssid} (${n.rssi} dBm) ${n.open?'[Open]':'[Secured]'}</div>`).join('')});</script>"
        "</div></body></html>";
    
    httpd_resp_send(req, html_page, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* Wi-Fi configuration GET handler */
static esp_err_t wifi_config_get_handler(httpd_req_t *req)
{
    const char* resp_str = "<!DOCTYPE html><html><head><title>Wi-Fi Config</title></head>"
                          "<body><h1>Wi-Fi Configuration</h1>"
                          "<form method=\"post\" action=\"/wifi\">"
                          "SSID: <input type=\"text\" name=\"ssid\" required><br><br>"
                          "Password: <input type=\"password\" name=\"password\"><br><br>"
                          "<input type=\"submit\" value=\"Connect\">"
                          "</form></body></html>";

    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t wifi_scan_get_handler(httpd_req_t *req)
{
    wifi_scan_result_t scan_results[20];
    size_t result_count = 20;
    
    esp_err_t ret = wifi_manager_scan_networks(scan_results, &result_count);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Scan failed");
        return ESP_FAIL;
    }
    
    // Build JSON response
    char *json_response = malloc(2048);
    if (!json_response) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_FAIL;
    }
    
    strcpy(json_response, "{\"networks\":[");
    for (size_t i = 0; i < result_count; i++) {
        char network_json[128];
        snprintf(network_json, sizeof(network_json),
                "%s{\"ssid\":\"%.32s\",\"rssi\":%d,\"open\":%s}",
                (i > 0) ? "," : "",
                scan_results[i].ssid,
                scan_results[i].rssi,
                scan_results[i].is_open ? "true" : "false");
        strcat(json_response, network_json);
    }
    strcat(json_response, "]}");
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_response, strlen(json_response));
    free(json_response);
    
    return ESP_OK;
}

static esp_err_t wifi_config_post_handler(httpd_req_t *req)
{
    char content[512];
    size_t recv_size = MIN(req->content_len, sizeof(content) - 1);
    
    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    content[ret] = '\0';
    
    // Parse form data (simple parsing for ssid and password)
    char ssid[64] = {0};
    char password[64] = {0};
    
    char *ssid_start = strstr(content, "ssid=");
    char *pass_start = strstr(content, "password=");
    
    if (ssid_start) {
        ssid_start += 5; // Skip "ssid="
        char *ssid_end = strchr(ssid_start, '&');
        if (!ssid_end) ssid_end = content + strlen(content);
        size_t ssid_len = MIN(ssid_end - ssid_start, sizeof(ssid) - 1);
        strncpy(ssid, ssid_start, ssid_len);
    }
    
    if (pass_start) {
        pass_start += 9; // Skip "password="
        char *pass_end = strchr(pass_start, '&');
        if (!pass_end) pass_end = content + strlen(content);
        size_t pass_len = MIN(pass_end - pass_start, sizeof(password) - 1);
        strncpy(password, pass_start, pass_len);
    }
    
    ESP_LOGI(TAG, "Received Wi-Fi config: SSID=%s", ssid);
    
    // URL decode (basic implementation)
    // In production, use proper URL decoding
    
    // Attempt to connect
    esp_err_t connect_result = wifi_manager_connect(ssid, password);
    
    const char* response = (connect_result == ESP_OK) ? 
        "Wi-Fi connection initiated. Please wait..." : 
        "Failed to start Wi-Fi connection.";
    
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* Start HTTP server */
esp_err_t wifi_http_server_start(void)
{
    if (http_server != NULL) {
        return ESP_OK; // Already running
    }
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = HTTP_SERVER_PORT;
    config.max_open_sockets = 7;
    
    ESP_LOGI(TAG, "Starting HTTP server on port %d", config.server_port);
    
    if (httpd_start(&http_server, &config) == ESP_OK) {
        // Register URI handlers
        httpd_uri_t root_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(http_server, &root_uri);
        
        httpd_uri_t wifi_config_get_uri = {
            .uri = "/wifi",
            .method = HTTP_GET,
            .handler = wifi_config_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(http_server, &wifi_config_get_uri);

        httpd_uri_t wifi_config_post_uri = {
            .uri = "/wifi",
            .method = HTTP_POST,
            .handler = wifi_config_post_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(http_server, &wifi_config_post_uri);
        
        httpd_uri_t scan_uri = {
            .uri = "/scan",
            .method = HTTP_GET,
            .handler = wifi_scan_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(http_server, &scan_uri);
        
        ESP_LOGI(TAG, "HTTP server started successfully");
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "Error starting HTTP server!");
    return ESP_FAIL;
}

/* Stop HTTP server */
esp_err_t wifi_http_server_stop(void)
{
    if (http_server) {
        httpd_stop(http_server);
        http_server = NULL;
        ESP_LOGI(TAG, "HTTP server stopped");
    }
    return ESP_OK;
}

/* Check if HTTP server is running */
bool wifi_http_server_is_running(void)
{
    return (http_server != NULL);
}

/* Start ECU data server */
esp_err_t wifi_start_ecu_server(void)
{
    ESP_LOGI(TAG, "ECU data server functionality integrated into main HTTP server");
    return ESP_OK;
}

/* Send ECU data via HTTP/WebSocket */
esp_err_t wifi_send_ecu_data(const char *json_data)
{
    // This would typically send data via WebSocket to connected clients
    // For now, just log the data
    ESP_LOGD(TAG, "ECU Data: %s", json_data);
    return ESP_OK;
}

/* Disconnect from Wi-Fi */
esp_err_t wifi_manager_disconnect(void)
{
    ESP_LOGI(TAG, "Disconnecting from Wi-Fi...");
    esp_err_t ret = esp_wifi_disconnect();
    current_status = WIFI_STATUS_DISCONNECTED;
    return ret;
}

/* Deinitialize Wi-Fi Manager */
esp_err_t wifi_manager_deinit(void)
{
    wifi_http_server_stop();
    esp_wifi_stop();
    esp_wifi_deinit();
    
    if (s_wifi_event_group) {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
    }
    
    ESP_LOGI(TAG, "Wi-Fi Manager deinitialized");
    return ESP_OK;
}