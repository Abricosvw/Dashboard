/*
 * Web Server for ECU Dashboard
 * Serves React application files and handles API requests
 */

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "WEB_SERVER";

// Embedded HTML for ECU Dashboard (минифицированная версия)
const char dashboard_html[] = R"HTML(
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ECU Dashboard</title>
    <style>
        body { 
            margin: 0; 
            padding: 20px; 
            background: linear-gradient(135deg, #0a0a0a 0%, #1a1a1a 100%);
            color: #fff; 
            font-family: 'Orbitron', monospace; 
            min-height: 100vh;
        }
        .dashboard { 
            display: grid; 
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); 
            gap: 20px; 
            max-width: 1200px; 
            margin: 0 auto; 
        }
        .gauge {
            background: radial-gradient(circle, #1a1a1a 0%, #0a0a0a 100%);
            border: 2px solid #333;
            border-radius: 15px;
            padding: 20px;
            text-align: center;
            position: relative;
            box-shadow: 0 0 20px rgba(0, 212, 255, 0.1);
        }
        .gauge-title { 
            font-size: 14px; 
            margin-bottom: 10px; 
            color: #00D4FF; 
            text-transform: uppercase; 
        }
        .gauge-value { 
            font-size: 32px; 
            font-weight: bold; 
            margin: 10px 0; 
        }
        .gauge-unit { 
            font-size: 12px; 
            color: #888; 
        }
        .gauge-canvas {
            width: 150px;
            height: 150px;
            margin: 0 auto;
        }
        .status-bar {
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            background: rgba(0, 0, 0, 0.9);
            padding: 10px;
            text-align: center;
            z-index: 1000;
        }
        .connection-status {
            color: #00FF88;
            animation: pulse 2s infinite;
        }
        @keyframes pulse {
            0% { opacity: 1; }
            50% { opacity: 0.5; }
            100% { opacity: 1; }
        }
        h1 { 
            text-align: center; 
            color: #00D4FF; 
            margin-top: 60px;
            text-shadow: 0 0 10px rgba(0, 212, 255, 0.5);
        }
    </style>
</head>
<body>
    <div class="status-bar">
        <span class="connection-status">🔗 ESP32-S3 ECU Dashboard</span>
        <span id="status">Подключение...</span>
    </div>
    
    <h1>🏎️ ECU DASHBOARD</h1>
    
    <div class="dashboard">
        <div class="gauge">
            <div class="gauge-title">MAP Pressure</div>
            <canvas class="gauge-canvas" id="mapCanvas"></canvas>
            <div class="gauge-value" id="mapValue">0</div>
            <div class="gauge-unit">kPa</div>
        </div>
        
        <div class="gauge">
            <div class="gauge-title">Wastegate</div>
            <canvas class="gauge-canvas" id="wastegateCanvas"></canvas>
            <div class="gauge-value" id="wastegateValue">0</div>
            <div class="gauge-unit">%</div>
        </div>
        
        <div class="gauge">
            <div class="gauge-title">TPS Position</div>
            <canvas class="gauge-canvas" id="tpsCanvas"></canvas>
            <div class="gauge-value" id="tpsValue">0</div>
            <div class="gauge-unit">%</div>
        </div>
        
        <div class="gauge">
            <div class="gauge-title">Engine RPM</div>
            <canvas class="gauge-canvas" id="rpmCanvas"></canvas>
            <div class="gauge-value" id="rpmValue">0</div>
            <div class="gauge-unit">RPM</div>
        </div>
        
        <div class="gauge">
            <div class="gauge-title">Target Boost</div>
            <canvas class="gauge-canvas" id="boostCanvas"></canvas>
            <div class="gauge-value" id="boostValue">0</div>
            <div class="gauge-unit">kPa</div>
        </div>
        
        <div class="gauge">
            <div class="gauge-title">TCU Status</div>
            <div class="gauge-value" id="tcuValue" style="color: #00FF88;">OK</div>
            <div class="gauge-unit">Protection Mode</div>
        </div>
    </div>

    <script>
        // WebSocket подключение
        const protocol = location.protocol === 'https:' ? 'wss:' : 'ws:';
        const ws = new WebSocket(`${protocol}//${location.host}/ws`);
        
        ws.onopen = function() {
            document.getElementById('status').textContent = 'Подключено ✓';
            document.getElementById('status').style.color = '#00FF88';
        };
        
        ws.onmessage = function(event) {
            const data = JSON.parse(event.data);
            if (data.type === 'ecuData') {
                updateGauges(data.data);
            }
        };
        
        ws.onclose = function() {
            document.getElementById('status').textContent = 'Отключено ✗';
            document.getElementById('status').style.color = '#FF3366';
        };
        
        // Функция обновления датчиков
        function updateGauges(data) {
            document.getElementById('mapValue').textContent = data.mapPressure || 0;
            document.getElementById('wastegateValue').textContent = data.wastegatePosition || 0;
            document.getElementById('tpsValue').textContent = data.tpsPosition || 0;
            document.getElementById('rpmValue').textContent = data.engineRpm || 0;
            document.getElementById('boostValue').textContent = data.targetBoost || 0;
            
            // TCU Status
            const tcuElement = document.getElementById('tcuValue');
            if (data.tcuLimpMode) {
                tcuElement.textContent = 'LIMP';
                tcuElement.style.color = '#FF3366';
            } else if (data.tcuProtectionActive) {
                tcuElement.textContent = 'PROTECT';
                tcuElement.style.color = '#FFD700';
            } else {
                tcuElement.textContent = 'OK';
                tcuElement.style.color = '#00FF88';
            }
            
            // Рисуем круглые датчики
            drawGauge('mapCanvas', data.mapPressure, 100, 250, '#00D4FF');
            drawGauge('wastegateCanvas', data.wastegatePosition, 0, 100, '#00FF88');
            drawGauge('tpsCanvas', data.tpsPosition, 0, 100, '#FFD700');
            drawGauge('rpmCanvas', data.engineRpm, 0, 7000, '#FF6B35');
            drawGauge('boostCanvas', data.targetBoost, 100, 250, '#FFD700');
        }
        
        // Функция рисования круглого датчика
        function drawGauge(canvasId, value, min, max, color) {
            const canvas = document.getElementById(canvasId);
            const ctx = canvas.getContext('2d');
            const centerX = canvas.width / 2;
            const centerY = canvas.height / 2;
            const radius = 60;
            
            // Очистка canvas
            ctx.clearRect(0, 0, canvas.width, canvas.height);
            
            // Нормализованное значение
            const normalized = Math.max(0, Math.min(1, (value - min) / (max - min)));
            
            // Фоновая дуга
            ctx.beginPath();
            ctx.arc(centerX, centerY, radius, Math.PI * 0.75, Math.PI * 2.25);
            ctx.strokeStyle = '#333';
            ctx.lineWidth = 8;
            ctx.stroke();
            
            // Значение дуги
            const endAngle = Math.PI * 0.75 + (Math.PI * 1.5 * normalized);
            ctx.beginPath();
            ctx.arc(centerX, centerY, radius, Math.PI * 0.75, endAngle);
            ctx.strokeStyle = color;
            ctx.lineWidth = 8;
            ctx.lineCap = 'round';
            ctx.stroke();
        }
        
        // Инициализация canvas элементов
        window.onload = function() {
            const canvases = document.querySelectorAll('.gauge-canvas');
            canvases.forEach(canvas => {
                canvas.width = 150;
                canvas.height = 150;
            });
            
            // Рисуем начальные датчики
            drawGauge('mapCanvas', 0, 100, 250, '#00D4FF');
            drawGauge('wastegateCanvas', 0, 0, 100, '#00FF88');
            drawGauge('tpsCanvas', 0, 0, 100, '#FFD700');
            drawGauge('rpmCanvas', 0, 0, 7000, '#FF6B35');
            drawGauge('boostCanvas', 0, 100, 250, '#FFD700');
        };
    </script>
</body>
</html>
)HTML";

// HTTP обработчик для главной страницы
static esp_err_t dashboard_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, dashboard_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Запуск веб-сервера для дашборда
esp_err_t start_dashboard_web_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_open_sockets = 10;
    
    httpd_handle_t server = NULL;
    
    ESP_LOGI(TAG, "Starting dashboard web server on port: %d", config.server_port);
    
    if (httpd_start(&server, &config) == ESP_OK) {
        // Регистрируем обработчик главной страницы
        httpd_uri_t dashboard_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = dashboard_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &dashboard_uri);
        
        // Также обрабатываем /dashboard
        httpd_uri_t dashboard_alt_uri = {
            .uri = "/dashboard",
            .method = HTTP_GET,
            .handler = dashboard_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &dashboard_alt_uri);
        
        ESP_LOGI(TAG, "Dashboard web server started successfully");
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "Error starting dashboard web server");
    return ESP_FAIL;
}