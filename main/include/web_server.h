/*
 * Web Server Header for ECU Dashboard
 */

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_err.h"

/**
 * @brief Start the dashboard web server
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t start_dashboard_web_server(void);

#endif // WEB_SERVER_H