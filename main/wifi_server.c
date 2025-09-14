#include "include/wifi_server.h"
#include "include/ecu_data.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "esp_wifi_types.h"
#include <string.h>
#include <stdlib.h>

static const char *WIFI_TAG = "WIFI_SERVER";

// Default WiFi configuration
#define DEFAULT_SSID "ECU_Dashboard"
#define DEFAULT_PASSWORD ""  // No password - open network

// Global variables
static httpd_handle_t server = NULL;
static wifi_config_t g_wifi_config = {0};
static bool g_ap_mode = true;
static char g_ip_address[16] = "192.168.4.1";
static bool wifi_connected = false;

// WiFi event handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(WIFI_TAG, "Station "MACSTR" joined, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(WIFI_TAG, "Station "MACSTR" left, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(WIFI_TAG, "Disconnected from WiFi, retrying...");
        esp_wifi_connect();
        wifi_connected = false;
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(WIFI_TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        snprintf(g_ip_address, sizeof(g_ip_address), 
                 IPSTR, IP2STR(&event->ip_info.ip));
        wifi_connected = true;
    }
}

esp_err_t wifi_server_init(void)
{
    // Initialize default configuration
    strcpy((char*)g_wifi_config.ap.ssid, DEFAULT_SSID);
    // No password for open network
    g_ap_mode = true;
    strcpy(g_ip_address, "192.168.4.1");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize TCP/IP
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Create WiFi interfaces
    if (g_ap_mode) {
        esp_netif_create_default_wifi_ap();
    } else {
        esp_netif_create_default_wifi_sta();
    }
    
    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
    
    // Configure WiFi
    if (g_ap_mode) {
        wifi_config_t wifi_config = {
            .ap = {
                .ssid_len = strlen((char*)g_wifi_config.ap.ssid),
                .channel = 1,
                .max_connection = 4,
                .authmode = WIFI_AUTH_OPEN
            },
        };
        strcpy((char*)wifi_config.ap.ssid, (char*)g_wifi_config.ap.ssid);
        // No password needed for open network
        
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    } else {
        wifi_config_t wifi_config = {
            .sta = {
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            },
        };
        strcpy((char*)wifi_config.sta.ssid, (char*)g_wifi_config.sta.ssid);
        strcpy((char*)wifi_config.sta.password, (char*)g_wifi_config.sta.password);
        
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    }
    
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(WIFI_TAG, "WiFi initialized in %s mode", g_ap_mode ? "AP" : "STA");
    return ESP_OK;
}

// HTTP handlers
esp_err_t handle_root(httpd_req_t *req)
{
    // Return the graphical dashboard directly
    const char* html_content = "<!DOCTYPE html>"
        "<html lang='en'>"
        "<head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
        "<title>ECU Dashboard - Wastegate Control</title>"
        "<link rel='preconnect' href='https://fonts.googleapis.com'>"
        "<link rel='preconnect' href='https://fonts.gstatic.com' crossorigin>"
        "<link href='https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&family=Orbitron:wght@400;500;600;700;800;900&display=swap' rel='stylesheet'>"
        "<link rel='stylesheet' href='/dashboard.css'>"
        "</head>"
        "<body>"
        "<div class='dashboard'>"
        "<header class='header'>"
        "<div class='header-left'>"
        "<h1 class='title'>🚗 ECU Dashboard</h1>"
        "<p class='subtitle'>Wastegate Control System</p>"
        "</div>"
        "<div class='header-right'>"
        "<div class='status-indicator' id='status-indicator'>"
        "<div class='status-dot'></div>"
        "<span class='status-text'>Connecting...</span>"
        "</div>"
        "</div>"
        "</header>"
        "<main class='main-content'>"
        "<div class='gauges-grid' id='gauges-grid'>"
        "<div class='gauge-container' data-gauge='rpm'>"
        "<div class='gauge' id='gauge-rpm'>"
        "<canvas width='200' height='200'></canvas>"
        "<div class='gauge-value'>0</div>"
        "<div class='gauge-label'>ENGINE RPM</div>"
        "<div class='gauge-subtitle'>Revolutions/min</div>"
        "</div>"
        "</div>"
        "<div class='gauge-container' data-gauge='map'>"
        "<div class='gauge' id='gauge-map'>"
        "<canvas width='200' height='200'></canvas>"
        "<div class='gauge-value'>0</div>"
        "<div class='gauge-label'>BOOST PRESSURE</div>"
        "<div class='gauge-subtitle'>MAP Sensor (kPa)</div>"
        "</div>"
        "</div>"
        "<div class='gauge-container' data-gauge='tps'>"
        "<div class='gauge' id='gauge-tps'>"
        "<canvas width='200' height='200'></canvas>"
        "<div class='gauge-value'>0</div>"
        "<div class='gauge-label'>THROTTLE</div>"
        "<div class='gauge-subtitle'>TPS (%)</div>"
        "</div>"
        "</div>"
        "<div class='gauge-container' data-gauge='wastegate'>"
        "<div class='gauge' id='gauge-wastegate'>"
        "<canvas width='200' height='200'></canvas>"
        "<div class='gauge-value'>0</div>"
        "<div class='gauge-label'>WASTEGATE</div>"
        "<div class='gauge-subtitle'>Position (%)</div>"
        "</div>"
        "</div>"
        "<div class='gauge-container' data-gauge='boost'>"
        "<div class='gauge' id='gauge-boost'>"
        "<canvas width='200' height='200'></canvas>"
        "<div class='gauge-value'>0</div>"
        "<div class='gauge-label'>BOOST</div>"
        "<div class='gauge-subtitle'>Pressure (PSI)</div>"
        "</div>"
        "</div>"
        "<div class='tcu-status'>"
        "<div class='tcu-indicator' id='tcu-indicator'>"
        "<div class='tcu-led'></div>"
        "<div class='tcu-label'>TCU Status</div>"
        "</div>"
        "</div>"
        "</div>"
        "<div class='data-stream' id='data-stream'>"
        "<h3>Data Stream</h3>"
        "<div class='stream-content' id='stream-content'></div>"
        "</div>"
        "</main>"
        "</div>"
        "<script src='/dashboard.js'></script>"
        "</body>"
        "</html>";
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, html_content, strlen(html_content));
    return ESP_OK;
}

esp_err_t handle_api_ecu_data(httpd_req_t *req)
{
    // Add CORS headers
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    
    char* json_str = ecu_data_to_string(ecu_data_get());
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    return ESP_OK;
}

esp_err_t handle_api_datastream(httpd_req_t *req)
{
    // Add CORS headers
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    
    char* json_str = data_stream_to_string();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    return ESP_OK;
}

// CORS preflight handler
esp_err_t handle_options(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    httpd_resp_send(req, "", 0);
    return ESP_OK;
}

// Static file handlers
esp_err_t handle_static_files(httpd_req_t *req)
{
    const char* uri = req->uri;
    
    // Handle specific files
    if (strcmp(uri, "/dashboard.css") == 0) {
        const char* css_content = "/* ECU Dashboard Styles - Automotive Theme */\n"
            ":root {\n"
            "  --automotive-bg: hsl(0, 0%, 4%);\n"
            "  --automotive-card: hsl(0, 0%, 10%);\n"
            "  --automotive-accent: hsl(195, 100%, 50%);\n"
            "  --automotive-warning: hsl(20, 100%, 60%);\n"
            "  --automotive-success: hsl(150, 100%, 55%);\n"
            "  --automotive-danger: hsl(345, 100%, 60%);\n"
            "  --automotive-yellow: hsl(50, 100%, 50%);\n"
            "  --automotive-text: hsl(0, 0%, 90%);\n"
            "  --automotive-text-secondary: hsl(0, 0%, 70%);\n"
            "  --automotive-border: hsl(0, 0%, 15%);\n"
            "}\n"
            "* { margin: 0; padding: 0; box-sizing: border-box; }\n"
            "body { font-family: 'Inter', sans-serif; background: linear-gradient(135deg, var(--automotive-bg) 0%, var(--automotive-card) 100%); color: var(--automotive-text); overflow: hidden; height: 100vh; }\n"
            ".dashboard { height: 100vh; display: flex; flex-direction: column; padding: 20px; }\n"
            ".header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; padding-bottom: 15px; border-bottom: 2px solid var(--automotive-border); }\n"
            ".header-left .title { font-family: 'Orbitron', monospace; font-size: 28px; font-weight: 700; color: var(--automotive-accent); text-shadow: 0 0 10px var(--automotive-accent); }\n"
            ".header-left .subtitle { font-size: 14px; color: var(--automotive-text-secondary); margin-top: 5px; }\n"
            ".status-indicator { display: flex; align-items: center; gap: 10px; padding: 8px 16px; background: rgba(0, 0, 0, 0.3); border-radius: 20px; border: 1px solid var(--automotive-border); }\n"
            ".status-dot { width: 12px; height: 12px; border-radius: 50%; background: var(--automotive-warning); animation: pulse 2s infinite; }\n"
            ".status-text { font-size: 14px; font-weight: 500; }\n"
            "@keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.5; } }\n"
            ".main-content { flex: 1; display: flex; flex-direction: column; }\n"
            ".gauges-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 20px; flex: 1; margin-bottom: 20px; }\n"
            ".gauge-container { display: flex; justify-content: center; align-items: center; }\n"
            ".gauge { position: relative; display: flex; flex-direction: column; align-items: center; background: radial-gradient(circle at center, var(--automotive-card) 0%, hsl(0, 0%, 6%) 100%); border-radius: 15px; padding: 20px; border: 2px solid var(--automotive-border); box-shadow: inset 0 0 20px hsla(195, 100%, 50%, 0.1), 0 0 30px hsla(0, 0%, 0%, 0.5); transition: all 0.3s ease; }\n"
            ".gauge:hover { transform: translateY(-2px); box-shadow: 0 8px 25px rgba(0, 0, 0, 0.3); border-color: var(--automotive-accent); }\n"
            ".gauge canvas { border-radius: 50%; background: rgba(0, 0, 0, 0.5); }\n"
            ".gauge-value { position: absolute; top: 50%; left: 50%; transform: translate(-50%, -50%); font-family: 'Orbitron', monospace; font-size: 24px; font-weight: 700; color: var(--automotive-accent); text-shadow: 0 0 10px var(--automotive-accent); }\n"
            ".gauge-label { margin-top: 10px; font-size: 14px; font-weight: 600; color: var(--automotive-text-secondary); text-transform: uppercase; letter-spacing: 1px; }\n"
            ".gauge-subtitle { font-size: 10px; color: var(--automotive-text-secondary); margin-top: 2px; }\n"
            ".tcu-status { display: flex; justify-content: center; align-items: center; }\n"
            ".tcu-indicator { display: flex; flex-direction: column; align-items: center; gap: 10px; padding: 20px; background: radial-gradient(circle at center, var(--automotive-card) 0%, hsl(0, 0%, 6%) 100%); border-radius: 15px; border: 2px solid var(--automotive-border); box-shadow: inset 0 0 20px hsla(195, 100%, 50%, 0.1), 0 0 30px hsla(0, 0%, 0%, 0.5); }\n"
            ".tcu-led { width: 30px; height: 30px; border-radius: 50%; background: var(--automotive-danger); box-shadow: 0 0 10px var(--automotive-danger); transition: all 0.3s ease; animation: pulse 2s infinite; }\n"
            ".tcu-label { font-size: 12px; font-weight: 600; color: var(--automotive-text-secondary); text-transform: uppercase; letter-spacing: 1px; }\n"
            ".data-stream { height: 150px; background: radial-gradient(circle at center, var(--automotive-card) 0%, hsl(0, 0%, 6%) 100%); border-radius: 10px; border: 2px solid var(--automotive-border); padding: 15px; box-shadow: inset 0 0 20px hsla(195, 100%, 50%, 0.1), 0 0 30px hsla(0, 0%, 0%, 0.5); }\n"
            ".data-stream h3 { font-size: 16px; font-weight: 600; color: var(--automotive-accent); margin-bottom: 10px; }\n"
            ".stream-content { height: calc(100% - 30px); overflow-y: auto; font-family: 'Courier New', monospace; font-size: 12px; }\n"
            ".stream-entry { display: flex; gap: 10px; padding: 5px 0; border-bottom: 1px solid rgba(255, 255, 255, 0.1); }\n"
            ".stream-entry:last-child { border-bottom: none; }\n"
            ".timestamp { color: var(--automotive-text-secondary); min-width: 80px; }\n"
            ".message { color: var(--automotive-text); }\n"
            ".stream-success .message { color: var(--automotive-success); }\n"
            ".stream-warning .message { color: var(--automotive-warning); }\n"
            ".stream-error .message { color: var(--automotive-danger); }\n";
        
        httpd_resp_set_type(req, "text/css");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, css_content, strlen(css_content));
        return ESP_OK;
    }
    
    if (strcmp(uri, "/dashboard.js") == 0) {
        const char* js_content = "class ECUDashboard {\n"
            "  constructor() {\n"
            "    this.data = {\n"
            "      engine_rpm: 0,\n"
            "      map_pressure: 0,\n"
            "      tps_position: 0,\n"
            "      wastegate_position: 0,\n"
            "      target_boost: 0,\n"
            "      tcu_protection_active: false,\n"
            "      tcu_limp_mode: false\n"
            "    };\n"
            "    this.gauges = [];\n"
            "    this.init();\n"
            "  }\n"
            "  init() {\n"
            "    this.initGauges();\n"
            "    this.startDataPolling();\n"
            "  }\n"
            "  initGauges() {\n"
            "    const gaugeConfigs = [\n"
            "      { id: 'rpm', max: 7000, unit: 'RPM', color: '#FF6B35', warningThreshold: 6000, dangerThreshold: 6500 },\n"
            "      { id: 'map', max: 250, unit: 'kPa', color: '#00D4FF', warningThreshold: 230, dangerThreshold: 245 },\n"
            "      { id: 'tps', max: 100, unit: '%', color: '#FFD700' },\n"
            "      { id: 'wastegate', max: 100, unit: '%', color: '#00FF88' },\n"
            "      { id: 'boost', max: 25, unit: 'PSI', color: '#96CEB4' }\n"
            "    ];\n"
            "    gaugeConfigs.forEach(config => {\n"
            "      this.createGauge(config);\n"
            "    });\n"
            "  }\n"
            "  createGauge(config) {\n"
            "    const canvas = document.querySelector(`#gauge-${config.id} canvas`);\n"
            "    if (!canvas) return;\n"
            "    const ctx = canvas.getContext('2d');\n"
            "    const centerX = canvas.width / 2;\n"
            "    const centerY = canvas.height / 2;\n"
            "    const radius = Math.min(centerX, centerY) - 20;\n"
            "    const gauge = {\n"
            "      id: config.id,\n"
            "      max: config.max,\n"
            "      unit: config.unit,\n"
            "      color: config.color,\n"
            "      value: 0,\n"
            "      ctx: ctx,\n"
            "      centerX: centerX,\n"
            "      centerY: centerY,\n"
            "      radius: radius,\n"
            "      warningThreshold: config.warningThreshold,\n"
            "      dangerThreshold: config.dangerThreshold\n"
            "    };\n"
            "    this.gauges.push(gauge);\n"
            "    this.drawGauge(gauge);\n"
            "  }\n"
            "  drawGauge(gauge) {\n"
            "    const ctx = gauge.ctx;\n"
            "    const centerX = gauge.centerX;\n"
            "    const centerY = gauge.centerY;\n"
            "    const radius = gauge.radius;\n"
            "    ctx.clearRect(0, 0, ctx.canvas.width, ctx.canvas.height);\n"
            "    const percentage = Math.max(0, Math.min(1, gauge.value / gauge.max));\n"
            "    const startAngle = -Math.PI / 2;\n"
            "    const endAngle = startAngle + (percentage * Math.PI * 2);\n"
            "    let strokeColor = gauge.color;\n"
            "    if (gauge.dangerThreshold && gauge.value >= gauge.dangerThreshold) {\n"
            "      strokeColor = '#FF3366';\n"
            "    } else if (gauge.warningThreshold && gauge.value >= gauge.warningThreshold) {\n"
            "      strokeColor = '#FF6B35';\n"
            "    }\n"
            "    ctx.beginPath();\n"
            "    ctx.arc(centerX, centerY, radius, 0, 2 * Math.PI);\n"
            "    ctx.strokeStyle = '#333';\n"
            "    ctx.lineWidth = 8;\n"
            "    ctx.stroke();\n"
            "    ctx.beginPath();\n"
            "    ctx.arc(centerX, centerY, radius, startAngle, endAngle);\n"
            "    ctx.strokeStyle = strokeColor;\n"
            "    ctx.lineWidth = 8;\n"
            "    ctx.lineCap = 'round';\n"
            "    ctx.stroke();\n"
            "    const valueElement = document.querySelector(`#gauge-${gauge.id} .gauge-value`);\n"
            "    if (valueElement) {\n"
            "      valueElement.textContent = Math.round(gauge.value);\n"
            "    }\n"
            "  }\n"
            "  updateGauge(id, value) {\n"
            "    const gauge = this.gauges.find(g => g.id === id);\n"
            "    if (gauge) {\n"
            "      gauge.value = value;\n"
            "      this.drawGauge(gauge);\n"
            "    }\n"
            "  }\n"
            "  updateTCUStatus(protection, limp) {\n"
            "    const indicator = document.getElementById('tcu-indicator');\n"
            "    if (!indicator) return;\n"
            "    const led = indicator.querySelector('.tcu-led');\n"
            "    if (led) {\n"
            "      if (protection || limp) {\n"
            "        led.style.backgroundColor = '#FF3366';\n"
            "        led.style.boxShadow = '0 0 10px #FF3366';\n"
            "      } else {\n"
            "        led.style.backgroundColor = '#00FF88';\n"
            "        led.style.boxShadow = '0 0 10px #00FF88';\n"
            "      }\n"
            "    }\n"
            "  }\n"
            "  updateStatusIndicator(connected) {\n"
            "    const indicator = document.getElementById('status-indicator');\n"
            "    if (!indicator) return;\n"
            "    const dot = indicator.querySelector('.status-dot');\n"
            "    const text = indicator.querySelector('.status-text');\n"
            "    if (dot && text) {\n"
            "      if (connected) {\n"
            "        dot.style.backgroundColor = '#00FF88';\n"
            "        text.textContent = 'Connected';\n"
            "      } else {\n"
            "        dot.style.backgroundColor = '#FF3366';\n"
            "        text.textContent = 'Disconnected';\n"
            "      }\n"
            "    }\n"
            "  }\n"
            "  addDataStreamEntry(message, type = 'info') {\n"
            "    const streamContent = document.getElementById('stream-content');\n"
            "    if (!streamContent) return;\n"
            "    const entry = document.createElement('div');\n"
            "    entry.className = `stream-entry stream-${type}`;\n"
            "    entry.innerHTML = `<span class=\"timestamp\">${new Date().toLocaleTimeString()}</span><span class=\"message\">${message}</span>`;\n"
            "    streamContent.appendChild(entry);\n"
            "    while (streamContent.children.length > 10) {\n"
            "      streamContent.removeChild(streamContent.firstChild);\n"
            "    }\n"
            "    streamContent.scrollTop = streamContent.scrollHeight;\n"
            "  }\n"
            "  async fetchECUData() {\n"
            "    try {\n"
            "      const response = await fetch('/api/ecu_data');\n"
            "      if (response.ok) {\n"
            "        const data = await response.json();\n"
            "        this.updateGauge('rpm', data.engine_rpm || 0);\n"
            "        this.updateGauge('map', data.map_pressure || 0);\n"
            "        this.updateGauge('tps', data.tps_position || 0);\n"
            "        this.updateGauge('wastegate', data.wastegate_position || 0);\n"
            "        this.updateGauge('boost', data.target_boost || 0);\n"
            "        this.updateTCUStatus(data.tcu_protection_active || false, data.tcu_limp_mode || false);\n"
            "        this.updateStatusIndicator(true);\n"
            "        this.addDataStreamEntry(`RPM: ${Math.round(data.engine_rpm || 0)}, MAP: ${(data.map_pressure || 0).toFixed(1)} kPa`, 'success');\n"
            "        return data;\n"
            "      }\n"
            "    } catch (error) {\n"
            "      console.error('Error fetching ECU data:', error);\n"
            "      this.updateStatusIndicator(false);\n"
            "      this.addDataStreamEntry('Connection error', 'error');\n"
            "    }\n"
            "    return null;\n"
            "  }\n"
            "  startDataPolling() {\n"
            "    this.fetchECUData();\n"
            "    setInterval(() => {\n"
            "      this.fetchECUData();\n"
            "    }, 100);\n"
            "  }\n"
            "}\n"
            "document.addEventListener('DOMContentLoaded', () => {\n"
            "  new ECUDashboard();\n"
            "});";
        
        httpd_resp_set_type(req, "application/javascript");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, js_content, strlen(js_content));
        return ESP_OK;
    }
    
    // This should never be reached since we only register specific URIs
    httpd_resp_set_status(req, "404 Not Found");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, "File not found", 13);
    
    return ESP_OK;
}

esp_err_t wifi_server_start(void)
{
    if (server != NULL) {
        return ESP_OK;
    }
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;
    
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Failed to start HTTP server");
        return ESP_FAIL;
    }
    
    // Register API URI handlers first
    httpd_uri_t api_ecu_uri = {
        .uri = "/api/ecu_data",
        .method = HTTP_GET,
        .handler = handle_api_ecu_data,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &api_ecu_uri);
    
    httpd_uri_t api_datastream_uri = {
        .uri = "/api/datastream",
        .method = HTTP_GET,
        .handler = handle_api_datastream,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &api_datastream_uri);
    
    // Add OPTIONS handler for CORS preflight
    httpd_uri_t options_uri = {
        .uri = "/*",
        .method = HTTP_OPTIONS,
        .handler = handle_options,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &options_uri);
    
    // Register root handler last to avoid conflicts
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = handle_root,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &root_uri);
    
    // Add static file handler for specific file types only
    httpd_uri_t static_uri = {
        .uri = "/dashboard.css",
        .method = HTTP_GET,
        .handler = handle_static_files,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &static_uri);
    
    httpd_uri_t js_uri = {
        .uri = "/dashboard.js",
        .method = HTTP_GET,
        .handler = handle_static_files,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &js_uri);
    
    ESP_LOGI(WIFI_TAG, "HTTP server started on port %d", config.server_port);
    return ESP_OK;
}

esp_err_t wifi_server_stop(void)
{
    if (server != NULL) {
        httpd_stop(&server);
        server = NULL;
        ESP_LOGI(WIFI_TAG, "HTTP server stopped");
    }
    return ESP_OK;
}

void wifi_server_broadcast_ecu_data(void)
{
    // For now, just log the data since we removed WebSocket
    ecu_data_t *data = ecu_data_get();
    ESP_LOGI(WIFI_TAG, "ECU Data: RPM=%.1f, MAP=%.1f, TPS=%.1f", 
              data->engine_rpm, data->map_kpa, data->tps_position);
}

bool wifi_is_connected(void)
{
    return wifi_connected;
}

int wifi_get_client_count(void)
{
    return 0; // Simplified for now
}

void wifi_server_set_config(const wifi_config_t *config)
{
    if (config != NULL) {
        memcpy(&g_wifi_config, config, sizeof(wifi_config_t));
    }
}

wifi_config_t* wifi_server_get_config(void)
{
    return &g_wifi_config;
}
