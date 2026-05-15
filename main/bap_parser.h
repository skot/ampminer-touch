/**
 * @file bap_parser.h
 * @brief BAP message parsing and UI integration
 */

#pragma once

#include "esp_err.h"
#include "bap_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Parse and handle a BAP message
 * @param message Raw BAP message string
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t bap_parse_and_handle_message(const char *message);

/**
 * @brief Handle a parsed BAP response message
 * @param msg Parsed BAP message structure
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t bap_handle_response(const bap_message_t *msg);

/**
 * @brief Handle hashrate response
 * @param value Hashrate value string
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t bap_handle_hashrate_response(const char *value);

/**
 * @brief Handle temperature response
 * @param value Temperature value string
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t bap_handle_temperature_response(const char *value);

/**
 * @brief Handle power response
 * @param value Power value string
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t bap_handle_power_response(const char *value);

/**
 * @brief Handle voltage response
 * @param value Voltage value string in millivolts
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t bap_handle_voltage_response(const char *value);

/**
 * @brief Handle fan RPM response
 * @param value Fan RPM value string
 * @return ESP_OK on success, error code otherwise
 
 */
esp_err_t bap_handle_fan_rpm_response(const char *value);

/**
 * @brief Handle fan percent setpoint response
 * @param value Fan speed percent string
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t bap_handle_fan_speed_percent_response(const char *value);

/**
 * @brief Handle share response
 * @param value Share value string
 * @return ESP_OK on success, error code otherwise
 * 
 */
 esp_err_t bap_handle_shares_response(const char *value);

/**
 * @brief Handle best difficulty response
 * @param value Best difficulty value string
 * @return ESP_OK on success, error code otherwise
 * 
 */
esp_err_t bap_handle_best_difficulty_response(const char *value);

/**
 * @brief Handle device model response
 * @param value Device model string
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t bap_handle_device_model_response(const char *value);

/**
 * @brief Handle ASIC model response
 * @param value ASIC model string
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t bap_handle_asic_model_response(const char *value);

/**
 * @brief Handle pool URL response
 * @param value Pool URL string
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t bap_handle_pool_url_response(const char *value);

/**
 * @brief Handle pool port response
 * @param value Pool port string
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t bap_handle_pool_port_response(const char *value);

/**
 * @brief Handle pool user response
 * @param value Pool user string
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t bap_handle_pool_user_response(const char *value);

/**
 * @brief Handle block height response
 * @param value Block height string
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t bap_handle_block_height_response(const char *value);

/**
 * @brief Handle mode response
 * @param value Mode string
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t bap_handle_mode(const char *value);

/**
 * @brief Handle miner IP response
 * @param value IP address string
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t bap_handle_miner_ip_response(const char *value);

/**
 * @brief Handle discovered Wi-Fi network response
 * @param value SSID string
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t bap_handle_wifi_network_response(const char *value);

/**
 * @brief Handle Wi-Fi scan/connect status response
 * @param value Status string
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t bap_handle_wifi_status_response(const char *value);

/**
 * @brief Handle Wi-Fi scan completion response
 * @param value Status string
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t bap_handle_wifi_scan_done_response(const char *value);

#ifdef __cplusplus
}
#endif
