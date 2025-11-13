/*
 * Simple Web Server for ECU Dashboard
 */

#include "esp_http_server.h"
#include "esp_log.h"
#include <string.h>
#include <math.h>
#include "include/can_websocket.h"
#include "ui/settings_config.h"

static const char *TAG = "WEB_SERVER";

// Simple HTML dashboard
const char dashboard_html[] = R"HTML(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Turbo Control Dashboard</title>
    <style>
        body { 
            margin: 0; 
            padding: 20px; 
            background: #0a0a0a;
            color: #fff; 
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            overflow-x: hidden;
        }
        
        .header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 20px;
            padding: 20px;
            background: #1a1a1a;
            border-radius: 10px;
            border: 1px solid #333;
        }
        
        .header-left {
            display: flex;
            align-items: center;
            gap: 20px;
        }
        
        .title {
            font-size: 28px;
            font-weight: bold;
            color: #fff;
        }
        
        .connection-status {
            display: flex;
            align-items: center;
            gap: 8px;
            color: #00FF88;
            font-size: 14px;
        }
        
        .status-dot {
            width: 8px;
            height: 8px;
            background: #00FF88;
            border-radius: 50%;
            animation: pulse 2s infinite;
        }
        
        .header-controls {
            display: flex;
            gap: 10px;
        }
        
        .control-btn {
            padding: 8px 16px;
            background: #2a2a2a;
            border: 1px solid #444;
            border-radius: 6px;
            color: #ccc;
            cursor: pointer;
            font-size: 12px;
            transition: all 0.3s ease;
        }
        
        .control-btn:hover {
            background: #3a3a3a;
            border-color: #555;
        }
        
        .status-bar {
            display: flex;
            justify-content: space-between;
            margin-bottom: 30px;
            padding: 15px 20px;
            background: #1a1a1a;
            border-radius: 10px;
            border: 1px solid #333;
            font-size: 14px;
        }
        
        .status-item {
            display: flex;
            align-items: center;
            gap: 8px;
        }
        
        .status-value {
            color: #00D4FF;
            font-weight: bold;
        }
        
        .dashboard {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 20px;
            max-width: 1200px;
            margin: 0 auto;
        }
        
        .gauge-panel {
            background: #1a1a1a;
            border: 1px solid #333;
            border-radius: 10px;
            padding: 20px;
            text-align: center;
            height: 300px;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: space-between;
        }
        
        .gauge-title {
            font-size: 16px;
            font-weight: bold;
            color: #fff;
            text-transform: uppercase;
            margin-bottom: 5px;
        }
        
        .gauge-subtitle {
            font-size: 12px;
            color: #aaa;
            margin-bottom: 15px;
        }
        
        .gauge-value-container {
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 10px;
            margin-bottom: 20px;
        }
        
        .gauge-value {
            font-size: 32px;
            font-weight: bold;
            color: #00D4FF;
        }
        
        .gauge-icon {
            width: 16px;
            height: 16px;
            background: #00D4FF;
            border-radius: 3px;
        }
        
        .gauge-unit {
            font-size: 14px;
            color: #888;
            margin-bottom: 15px;
        }
        
        .semi-circle-gauge {
            width: 120px;
            height: 60px;
            position: relative;
            margin-bottom: 15px;
        }
        
        .semi-circle-bg {
            fill: none;
            stroke: #444;
            stroke-width: 8;
        }
        
        .semi-circle-value {
            fill: none;
            stroke-width: 8;
            stroke-linecap: round;
            transition: stroke-dasharray 0.5s ease;
        }
        
        .gauge-pointer {
            position: absolute;
            width: 12px;
            height: 12px;
            background: #00FF88;
            border-radius: 50%;
            border: 2px solid #fff;
            box-shadow: 0 0 5px rgba(0, 255, 136, 0.5);
            transition: all 0.3s ease;
        }
        
        .gauge-info {
            font-size: 11px;
            color: #666;
            line-height: 1.4;
        }
        
        .tcu-panel {
            background: #1a1a1a;
            border: 1px solid #333;
            border-radius: 10px;
            padding: 20px;
            text-align: center;
            height: 300px;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: space-between;
        }
        
        .tcu-icon {
            width: 80px;
            height: 80px;
            background: #00D4FF;
            border-radius: 50%;
            display: flex;
            align-items: center;
            justify-content: center;
            margin: 20px 0;
            position: relative;
        }
        
        .tcu-icon::before {
            content: "🛡️";
            font-size: 40px;
        }
        
        .tcu-status {
            font-size: 18px;
            font-weight: bold;
            color: #00D4FF;
            margin-bottom: 15px;
        }
        
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }
        
        .gauge-panel:hover {
            border-color: #00D4FF;
            box-shadow: 0 0 15px rgba(0, 212, 255, 0.2);
        }
    </style>
</head>
<body>
    <div class="header">
        <div class="header-left">
            <div class="title">Turbo Control Dashboard</div>
            <div class="connection-status">
                <div class="status-dot"></div>
                Connected to ECU
            </div>
        </div>
        <div class="header-controls">
            <button class="control-btn">⚙️ Display Settings</button>
            <button class="control-btn">⚙️ Settings</button>
            <button class="control-btn">🔴 Record</button>
        </div>
    </div>
    
    <div class="status-bar">
        <div class="status-item">
            <span>TCU Status:</span>
            <span class="status-value">Unknown</span>
        </div>
        <div class="status-item">
            <span>Protection:</span>
            <span class="status-value">Unknown</span>
        </div>
        <div class="status-item">
            <span>Data Rate:</span>
            <span class="status-value">20Hz</span>
        </div>
        <div class="status-item">
            <span>Last Update:</span>
            <span class="status-value">Never</span>
        </div>
    </div>
    
    <div class="dashboard">
        <!-- BOOST PRESSURE -->
        <div class="gauge-panel">
            <div class="gauge-title">BOOST PRESSURE</div>
            <div class="gauge-subtitle">MAP Sensor (kPa)</div>
            <div class="gauge-value-container">
                <div class="gauge-value" id="mapValue">0.0</div>
                <div class="gauge-icon"></div>
            </div>
            <div class="gauge-unit">kPa</div>
            <div class="semi-circle-gauge">
                <svg width="120" height="60" viewBox="0 0 120 60">
                    <path class="semi-circle-bg" d="M 20 50 A 40 40 0 0 1 100 50"></path>
                    <path class="semi-circle-value" d="M 20 50 A 40 40 0 0 1 20 50" 
                          stroke="#00FF88" id="mapGauge"></path>
                </svg>
                <div class="gauge-pointer" id="mapPointer" style="left: 20px; top: 50px;"></div>
            </div>
            <div class="gauge-info">
                Target: <span id="mapTarget">0.0</span> kPa<br>
                Range: 100-250 kPa
            </div>
        </div>
        
        <!-- WASTEGATE -->
        <div class="gauge-panel">
            <div class="gauge-title">WASTEGATE</div>
            <div class="gauge-subtitle">Position Control (%)</div>
            <div class="gauge-value-container">
                <div class="gauge-value" id="wastegateValue">0.0</div>
                <div class="gauge-icon"></div>
            </div>
            <div class="gauge-unit">%</div>
            <div class="semi-circle-gauge">
                <svg width="120" height="60" viewBox="0 0 120 60">
                    <path class="semi-circle-bg" d="M 20 50 A 40 40 0 0 1 100 50"></path>
                    <path class="semi-circle-value" d="M 20 50 A 40 40 0 0 1 20 50" 
                          stroke="#00FF88" id="wastegateGauge"></path>
                </svg>
                <div class="gauge-pointer" id="wastegatePointer" style="left: 20px; top: 50px;"></div>
            </div>
            <div class="gauge-info">
                Range: 0-100 %
            </div>
        </div>
        
        <!-- THROTTLE -->
        <div class="gauge-panel">
            <div class="gauge-title">THROTTLE</div>
            <div class="gauge-subtitle">TPS Position (%)</div>
            <div class="gauge-value-container">
                <div class="gauge-value" id="tpsValue">0.0</div>
                <div class="gauge-icon"></div>
            </div>
            <div class="gauge-unit">%</div>
            <div class="semi-circle-gauge">
                <svg width="120" height="60" viewBox="0 0 120 60">
                    <path class="semi-circle-bg" d="M 20 50 A 40 40 0 0 1 100 50"></path>
                    <path class="semi-circle-value" d="M 20 50 A 40 40 0 0 1 20 50" 
                          stroke="#FFD700" id="tpsGauge"></path>
                </svg>
                <div class="gauge-pointer" id="tpsPointer" style="left: 20px; top: 50px;"></div>
            </div>
            <div class="gauge-info">
                Range: 0-100 %
            </div>
        </div>
        
        <!-- ENGINE RPM -->
        <div class="gauge-panel">
            <div class="gauge-title">ENGINE RPM</div>
            <div class="gauge-subtitle">Engine Speed</div>
            <div class="gauge-value-container">
                <div class="gauge-value" id="rpmValue">0</div>
                <div class="gauge-icon"></div>
            </div>
            <div class="gauge-unit">RPM</div>
            <div class="semi-circle-gauge">
                <svg width="120" height="60" viewBox="0 0 120 60">
                    <path class="semi-circle-bg" d="M 20 50 A 40 40 0 0 1 100 50"></path>
                    <path class="semi-circle-value" d="M 20 50 A 40 40 0 0 1 20 50" 
                          stroke="#FF6B35" id="rpmGauge"></path>
                </svg>
                <div class="gauge-pointer" id="rpmPointer" style="left: 20px; top: 50px;"></div>
            </div>
            <div class="gauge-info">
                Range: 0-7000 RPM
            </div>
        </div>
        
        <!-- TARGET BOOST -->
        <div class="gauge-panel">
            <div class="gauge-title">TARGET BOOST</div>
            <div class="gauge-subtitle">Desired Pressure (kPa)</div>
            <div class="gauge-value-container">
                <div class="gauge-value" id="boostValue">0.0</div>
                <div class="gauge-icon"></div>
            </div>
            <div class="gauge-unit">kPa</div>
            <div class="semi-circle-gauge">
                <svg width="120" height="60" viewBox="0 0 120 60">
                    <path class="semi-circle-bg" d="M 20 50 A 40 40 0 0 1 100 50"></path>
                    <path class="semi-circle-value" d="M 20 50 A 40 40 0 0 1 20 50" 
                          stroke="#FFD700" id="boostGauge"></path>
                </svg>
                <div class="gauge-pointer" id="boostPointer" style="left: 20px; top: 50px;"></div>
            </div>
            <div class="gauge-info">
                Range: 100-250 kPa
            </div>
        </div>
        
        <!-- TCU STATUS -->
        <div class="tcu-panel">
            <div class="gauge-title">TCU STATUS</div>
            <div class="gauge-subtitle">Transmission Control</div>
            <div class="tcu-icon"></div>
            <div class="tcu-status" id="tcuStatus">UNKNOWN</div>
            <div class="gauge-info">
                Torque Req: <span id="torqueReq">0</span>%<br>
                Transmission Status
            </div>
        </div>
    </div>

    <script>
        // Gauge ranges and colors
        const gaugeConfig = {
            map: { min: 100, max: 250, color: '#00FF88', target: 180 },
            wastegate: { min: 0, max: 100, color: '#00FF88' },
            tps: { min: 0, max: 100, color: '#FFD700' },
            rpm: { min: 0, max: 7000, color: '#FF6B35' },
            boost: { min: 100, max: 250, color: '#FFD700' }
        };
        
        function updateSemiCircleGauge(gaugeId, pointerId, value, config) {
            const gauge = document.getElementById(gaugeId);
            const pointer = document.getElementById(pointerId);
            
            if (!gauge || !pointer) return;
            
            const percentage = Math.min(Math.max((value - config.min) / (config.max - config.min), 0), 1);
            const angle = percentage * Math.PI; // 0 to π (semi-circle)
            const radius = 40;
            
            // Calculate pointer position
            const x = 20 + radius * Math.cos(angle);
            const y = 50 - radius * Math.sin(angle);
            
            // Update gauge arc
            const arcLength = percentage * Math.PI * radius;
            gauge.style.strokeDasharray = `${arcLength} ${Math.PI * radius}`;
            
            // Update pointer position
            pointer.style.left = `${x}px`;
            pointer.style.top = `${y}px`;
            pointer.style.background = config.color;
        }
        
        function updateTCUStatus(rpm) {
            const status = document.getElementById('tcuStatus');
            const icon = document.querySelector('.tcu-icon');
            
            if (rpm > 5500) {
                status.textContent = 'ERROR';
                status.style.color = '#FF0000';
                icon.style.background = '#FF0000';
            } else if (rpm > 4500) {
                status.textContent = 'WARNING';
                status.style.color = '#FFAA00';
                icon.style.background = '#FFAA00';
            } else {
                status.textContent = 'OK';
                status.style.color = '#00FF88';
                icon.style.background = '#00FF88';
            }
        }
        
        function updateStatusBar() {
            const now = new Date();
            const timeStr = now.toLocaleTimeString();
            document.querySelector('.status-bar .status-item:last-child .status-value').textContent = timeStr;
        }
        
        function startDataPolling() {
            document.querySelector('.connection-status').innerHTML = '<div class="status-dot"></div>Connected to ECU';
            setInterval(fetchData, 1000);
            setInterval(updateStatusBar, 1000);
        }
        
        function fetchData() {
            fetch('/data')
                .then(response => response.json())
                .then(data => {
                    updateGauges(data);
                })
                .catch(error => {
                    console.error('Error:', error);
                });
        }
        
        function updateGauges(data) {
            // Update values
            document.getElementById('mapValue').textContent = (data.map_pressure || 0).toFixed(1);
            document.getElementById('wastegateValue').textContent = (data.wastegate_pos || 0).toFixed(1);
            document.getElementById('tpsValue').textContent = (data.tps_position || 0).toFixed(1);
            document.getElementById('rpmValue').textContent = data.engine_rpm || 0;
            document.getElementById('boostValue').textContent = (data.target_boost || 0).toFixed(1);
            
            // Update targets
            document.getElementById('mapTarget').textContent = (data.target_boost || 0).toFixed(1);
            document.getElementById('torqueReq').textContent = Math.round((data.wastegate_pos || 0) * 0.8);
            
            // Update gauges
            updateSemiCircleGauge('mapGauge', 'mapPointer', data.map_pressure || 0, gaugeConfig.map);
            updateSemiCircleGauge('wastegateGauge', 'wastegatePointer', data.wastegate_pos || 0, gaugeConfig.wastegate);
            updateSemiCircleGauge('tpsGauge', 'tpsPointer', data.tps_position || 0, gaugeConfig.tps);
            updateSemiCircleGauge('rpmGauge', 'rpmPointer', data.engine_rpm || 0, gaugeConfig.rpm);
            updateSemiCircleGauge('boostGauge', 'boostPointer', data.target_boost || 0, gaugeConfig.boost);
            
            // Update TCU status
            updateTCUStatus(data.engine_rpm || 0);
        }
        
        window.onload = function() {
            startDataPolling(); // Start data polling directly on page load
        };
    </script>
</body>
</html>
)HTML";

// HTTP handler for main page
static esp_err_t dashboard_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Dashboard handler called for URI: %s", req->uri);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, dashboard_html, HTTPD_RESP_USE_STRLEN);
    ESP_LOGI(TAG, "Dashboard HTML sent successfully");
    return ESP_OK;
}

// Handler for CAN data
static esp_err_t can_data_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "CAN data handler called for URI: %s, method: %d", req->uri, req->method);
    if (req->method == HTTP_GET) {
        // Check if demo mode is enabled
        bool demo_enabled = demo_mode_get_enabled();
        ESP_LOGI(TAG, "🌐 Web server - demo mode check: %s", demo_enabled ? "ENABLED" : "DISABLED");

        if (demo_enabled) {
            // Return simulated CAN data for demo (since WebSocket server has real data)
            char json_data[256];
            static int demo_counter = 0;
            demo_counter++;

            // Generate demo data using simple periodic functions
            int cycle = demo_counter % 100;
            float phase = cycle / 100.0f;
            float map_pressure = 120.0f + 30.0f * (cycle > 50 ? (100 - cycle) : cycle) / 50.0f;
            float wastegate_pos = 45.0f + 25.0f * phase;
            float tps_position = 35.0f + 30.0f * phase;
            float engine_rpm = 2500.0f + 500.0f * phase;
            float target_boost = 180.0f + 20.0f * phase;
            int tcu_status = (demo_counter % 100 > 95) ? 1 : 0; // Occasional warning

            snprintf(json_data, sizeof(json_data),
                "{\"map_pressure\":%.1f,\"wastegate_pos\":%.1f,\"tps_position\":%.1f,"
                "\"engine_rpm\":%.0f,\"target_boost\":%.1f,\"tcu_status\":%d}",
                map_pressure, wastegate_pos, tps_position, engine_rpm, target_boost, tcu_status);

            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
            httpd_resp_send(req, json_data, HTTPD_RESP_USE_STRLEN);
            ESP_LOGI(TAG, "CAN demo data JSON sent successfully");
        } else {
            // Demo mode disabled, return zero values
            char json_data[256];
            snprintf(json_data, sizeof(json_data),
                "{\"map_pressure\":0.0,\"wastegate_pos\":0.0,\"tps_position\":0.0,"
                "\"engine_rpm\":0,\"target_boost\":0.0,\"tcu_status\":0}");

            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
            httpd_resp_send(req, json_data, HTTPD_RESP_USE_STRLEN);
            ESP_LOGI(TAG, "❌ Demo mode disabled, zero values sent to client");
        }
        return ESP_OK;
    }
    ESP_LOGW(TAG, "Invalid method for CAN data handler");
    return ESP_FAIL;
}

// Start dashboard web server
esp_err_t start_dashboard_web_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_open_sockets = 7;
    
    httpd_handle_t server = NULL;
    
    ESP_LOGI(TAG, "Starting dashboard web server on port: %d", config.server_port);
    
    if (httpd_start(&server, &config) == ESP_OK) {
        // Register main page handler
        httpd_uri_t dashboard_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = dashboard_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &dashboard_uri);
        
        // Also handle /dashboard
        httpd_uri_t dashboard_alt_uri = {
            .uri = "/dashboard",
            .method = HTTP_GET,
            .handler = dashboard_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &dashboard_alt_uri);
        
        // Handler for CAN data
        httpd_uri_t can_data_uri = {
            .uri = "/data",
            .method = HTTP_GET,
            .handler = can_data_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &can_data_uri);
        
        ESP_LOGI(TAG, "Dashboard web server started successfully");
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "Error starting dashboard web server");
    return ESP_FAIL;
}
