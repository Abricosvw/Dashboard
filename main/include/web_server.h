#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_http_server.h"

/**
 * @brief Starts the main dashboard web server.
 *
 * @return ESP_OK on success, ESP_FAIL on failure.
 */
esp_err_t start_dashboard_web_server(void);

/**
 * @brief Gets the handle of the running web server.
 *
 * This function should only be called after start_dashboard_web_server()
 * has successfully completed.
 *
 * @return The httpd_handle_t of the server, or NULL if the server is not running.
 */
httpd_handle_t web_server_get_handle(void);

#endif // WEB_SERVER_H