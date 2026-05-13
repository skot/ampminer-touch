/**
 * @file bap_client.h
 * @brief BAP client main interface
 */

#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize BAP client
 * Creates CDC transport tasks and initializes communication
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t bap_client_init(void);

/**
 * @brief Subscribe to a parameter
 * @param parameter Parameter name to subscribe to
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t bap_client_subscribe(const char *parameter);

/**
 * @brief Request a parameter value
 * @param parameter Parameter name to request
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t bap_client_request(const char *parameter);

/**
 * @brief Send frequency setting
 * @param frequency_mhz Frequency in MHz
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t bap_client_send_frequency_setting(float frequency_mhz);

/**
 * @brief Send ASIC voltage setting
 * @param voltage Voltage value
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t bap_client_send_asic_voltage(float voltage);

/**
 * @brief Send fan speed setting
 * @param speed_percent Fan speed percentage (0-100)
 * @return ESP_OK on success, error code otherwise
 * 
 */
esp_err_t bap_client_send_fan_speed(int speed_percent);

/**
 * @brief Send automatic fan control setting
 * @param enabled true to enable, false to disable
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t bap_client_send_automatic_fan_control(bool enabled);

/**
 * @brief Send Wi-Fi SSID to the host-side control board service
 * @param ssid Wi-Fi network name
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t bap_client_send_wifi_ssid(const char *ssid);

/**
 * @brief Send Wi-Fi password to the host-side control board service
 * @param password Wi-Fi password
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t bap_client_send_wifi_password(const char *password);

/**
 * @brief Ask the host-side control board service to connect with the staged Wi-Fi credentials
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t bap_client_send_wifi_connect(void);

/**
 * @brief Check if BAP client is connected
 * @return true if connected (recent response received), false otherwise
 */
bool bap_client_is_connected(void);

/**
 * @brief Reset connection state
 * Used when connection is lost and needs to be re-established
 */
void bap_client_reset_connection_state(void);

/**
 * @brief Suspend BAP client tasks (for OTA updates)
 * Suspends transport receive and connection monitor tasks to prevent display updates
 */
void bap_client_suspend(void);

/**
 * @brief Resume BAP client tasks (after OTA updates)
 * Resumes transport receive and connection monitor tasks
 */
void bap_client_resume(void);

#ifdef __cplusplus
}
#endif
