/**
 * @file bap_client.c
 * @brief BAP client main logic, tasks, and connection management
 * 
 * Handles high-level BAP client operations, FreeRTOS tasks, and connection state
 */

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_task_wdt.h"
#include "bap_client.h"
#include "bap_protocol.h"
#include "bap_transport.h"
#include "bap_parser.h"

static const char *TAG = "BAP_CLIENT";

static bool subscriptions_sent = false;
static bool system_info_requested = false;
static bool subscribed_hashrate = false;
static bool subscribed_temperature = false;
static bool subscribed_power = false;
static bool subscribed_voltage = false;
static bool subscribed_fan_rpm = false;
static bool subscribed_fan_speed_percent = false;
static bool subscribed_shares = false;
static bool subscribed_best_difficulty = false;
static bool subscribed_block_height = false;
static uint32_t last_response_time = 0;

// Task handles for suspend/resume
static TaskHandle_t transport_receive_task_handle = NULL;
static TaskHandle_t connection_monitor_task_handle = NULL;

static void transport_send_task(void *pvParameters);
static void transport_receive_task(void *pvParameters);
static void connection_monitor_task(void *pvParameters);

esp_err_t bap_client_init(void) {
    ESP_LOGI(TAG, "BAP client initialization starting...");
    
    if (subscriptions_sent) {
        ESP_LOGW(TAG, "Subscriptions already sent, skipping initialization");
        return ESP_OK;
    }
    
    esp_err_t ret = bap_transport_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BAP transport initialization failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Creating BAP transport tasks...");

    BaseType_t task_ret = xTaskCreate(transport_send_task, "bap_tx", 4096, NULL, 5, NULL);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create BAP send task");
        return ESP_FAIL;
    }

    task_ret = xTaskCreate(transport_receive_task, "bap_rx", 4096, NULL, 5, &transport_receive_task_handle);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create BAP receive task");
        return ESP_FAIL;
    }

    task_ret = xTaskCreate(connection_monitor_task, "connection_monitor", 3072, NULL, 3, &connection_monitor_task_handle);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create connection monitor task");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "BAP client initialization completed successfully");
    return ESP_OK;
}

esp_err_t bap_client_subscribe(const char *parameter) {
    if (!parameter) {
        return ESP_ERR_INVALID_ARG;
    }
    
    char message[BAP_MAX_MESSAGE_LEN];
    esp_err_t ret = bap_format_message(message, sizeof(message), BAP_CMD_SUB, parameter, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to format subscription message for %s", parameter);
        return ret;
    }
    
    ret = bap_transport_write(message, strlen(message));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send subscription for %s", parameter);
        return ret;
    }
    
    ESP_LOGI(TAG, "Sent: %s", message);
    return ESP_OK;
}

esp_err_t bap_client_request(const char *parameter) {
    if (!parameter) {
        return ESP_ERR_INVALID_ARG;
    }
    
    char message[BAP_MAX_MESSAGE_LEN];
    esp_err_t ret = bap_format_message(message, sizeof(message), BAP_CMD_REQ, parameter, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to format request message for %s", parameter);
        return ret;
    }
    
    ret = bap_transport_write(message, strlen(message));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send request for %s", parameter);
        return ret;
    }
    
    ESP_LOGI(TAG, "Sent: %s", message);
    return ESP_OK;
}

esp_err_t bap_client_send_frequency_setting(float frequency_mhz) {
    char message[BAP_MAX_MESSAGE_LEN];
    esp_err_t ret = bap_format_frequency_message(message, sizeof(message), frequency_mhz);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to format frequency message");
        return ret;
    }
    
    ret = bap_transport_write(message, strlen(message));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send frequency setting");
        return ret;
    }
    
    ESP_LOGI(TAG, "Sent frequency setting: %s", message);
    return ESP_OK;
}

esp_err_t bap_client_send_asic_voltage(float voltage) {
    char message[BAP_MAX_MESSAGE_LEN];
    esp_err_t ret = bap_format_voltage_message(message, sizeof(message), voltage);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to format voltage message");
        return ret;
    }
    
    ret = bap_transport_write(message, strlen(message));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send voltage setting");
        return ret;
    }
    
    ESP_LOGI(TAG, "Sent ASIC voltage setting: %s", message);
    return ESP_OK;
}

esp_err_t bap_client_send_fan_speed(int speed_percent) {
    if (speed_percent < 0 || speed_percent > 100) {
        return ESP_ERR_INVALID_ARG;
    }
    
    char message[BAP_MAX_MESSAGE_LEN];
    esp_err_t ret = bap_format_fan_speed_message(message, sizeof(message), speed_percent);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to format fan speed message");
        return ret;
    }
    
    ret = bap_transport_write(message, strlen(message));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send fan speed setting");
        return ret;
    }
    
    ESP_LOGI(TAG, "Sent fan speed setting: %s", message);
    return ESP_OK;
}

esp_err_t bap_client_send_automatic_fan_control(bool enabled) {
    char message[BAP_MAX_MESSAGE_LEN];
    esp_err_t ret = bap_format_message(message, sizeof(message), BAP_CMD_SET, "auto_fan", enabled ? "1" : "0");
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to format auto fan control message");
        return ret;
    }
    
    ret = bap_transport_write(message, strlen(message));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send auto fan control setting");
        return ret;
    }
    
    ESP_LOGI(TAG, "Sent auto fan control setting: %s", message);
    return ESP_OK;
}

esp_err_t bap_client_send_wifi_ssid(const char *ssid) {
    if (!ssid || ssid[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    char message[BAP_MAX_MESSAGE_LEN];
    esp_err_t ret = bap_format_message(message, sizeof(message), BAP_CMD_SET, "wifi_ssid", ssid);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to format Wi-Fi SSID message");
        return ret;
    }

    ret = bap_transport_write(message, strlen(message));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send Wi-Fi SSID");
        return ret;
    }

    ESP_LOGI(TAG, "Sent Wi-Fi SSID setting");
    return ESP_OK;
}

esp_err_t bap_client_send_wifi_password(const char *password) {
    if (!password) {
        return ESP_ERR_INVALID_ARG;
    }

    char message[BAP_MAX_MESSAGE_LEN];
    esp_err_t ret = bap_format_message(message, sizeof(message), BAP_CMD_SET, "wifi_password", password);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to format Wi-Fi password message");
        return ret;
    }

    ret = bap_transport_write(message, strlen(message));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send Wi-Fi password");
        return ret;
    }

    ESP_LOGI(TAG, "Sent Wi-Fi password setting");
    return ESP_OK;
}

esp_err_t bap_client_send_wifi_connect(void) {
    char message[BAP_MAX_MESSAGE_LEN];
    esp_err_t ret = bap_format_message(message, sizeof(message), BAP_CMD_SET, "wifi_connect", "1");
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to format Wi-Fi connect message");
        return ret;
    }

    ret = bap_transport_write(message, strlen(message));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send Wi-Fi connect request");
        return ret;
    }

    ESP_LOGI(TAG, "Sent Wi-Fi connect request");
    return ESP_OK;
}

bool bap_client_is_connected(void) {
    if (last_response_time == 0) return false;
    uint32_t current_time = xTaskGetTickCount();
    return (current_time - last_response_time) < pdMS_TO_TICKS(30000);
}

void bap_client_reset_connection_state(void) {
    ESP_LOGI(TAG, "Resetting BAP connection state");
    subscriptions_sent = false;
    system_info_requested = false;
    subscribed_hashrate = false;
    subscribed_temperature = false;
    subscribed_power = false;
    subscribed_voltage = false;
    subscribed_fan_rpm = false;
    subscribed_fan_speed_percent = false;
    subscribed_shares = false;
    subscribed_best_difficulty = false;
    subscribed_block_height = false;
    last_response_time = 0;
}

static esp_err_t bap_subscribe_hashrate(void) {
    if (subscribed_hashrate) {
        ESP_LOGW(TAG, "Already subscribed to hashrate, skipping");
        return ESP_OK;
    }
    
    esp_err_t ret = bap_client_subscribe("hashrate");
    if (ret == ESP_OK) {
        subscribed_hashrate = true;
        ESP_LOGI(TAG, "Subscribed to hashrate");
    }
    return ret;
}

static esp_err_t bap_subscribe_shares(void) {
    if(subscribed_shares) {
        ESP_LOGW(TAG, "Already subscribed to shares, skipping");
        return ESP_OK;
    }

    esp_err_t ret = bap_client_subscribe("shares");
    if (ret == ESP_OK) {
        subscribed_shares = true;
        ESP_LOGI(TAG, "Subscribed to shares");
    }
    return ret;
}

static esp_err_t bap_subscribe_block_height(void) {
    if (subscribed_block_height) {
        ESP_LOGW(TAG, "Already subscribed to block height, skipping");
        return ESP_OK;
    }

    esp_err_t ret = bap_client_subscribe("block_height");
    if (ret == ESP_OK) {
        subscribed_block_height = true;
        ESP_LOGI(TAG, "Subscribed to block height");
    }
    return ret;
}

static esp_err_t bap_subscribe_best_difficulty(void) {
    if (subscribed_best_difficulty) {
        ESP_LOGW(TAG, "Already subscribed to best difficulty, skipping");
        return ESP_OK;
    }
    esp_err_t ret = bap_client_subscribe("best_difficulty");
    if (ret == ESP_OK) {
        subscribed_best_difficulty = true;
        ESP_LOGI(TAG, "Subscribed to best difficulty");
    }
    return ret;
}

static esp_err_t bap_subscribe_temperature(void) {
    if (subscribed_temperature) {
        ESP_LOGW(TAG, "Already subscribed to temperature, skipping");
        return ESP_OK;
    }
    
    esp_err_t ret = bap_client_subscribe("temperature");
    if (ret == ESP_OK) {
        subscribed_temperature = true;
        ESP_LOGI(TAG, "Subscribed to temperature");
    }
    return ret;
}

static esp_err_t bap_subscribe_power(void) {
    if (subscribed_power) {
        ESP_LOGW(TAG, "Already subscribed to power, skipping");
        return ESP_OK;
    }
    
    esp_err_t ret = bap_client_subscribe("power");
    if (ret == ESP_OK) {
        subscribed_power = true;
        ESP_LOGI(TAG, "Subscribed to power");
    }
    return ret;
}

static esp_err_t bap_subscribe_voltage(void) {
    if (subscribed_voltage) {
        ESP_LOGW(TAG, "Already subscribed to voltage, skipping");
        return ESP_OK;
    }

    esp_err_t ret = bap_client_subscribe("voltage");
    if (ret == ESP_OK) {
        subscribed_voltage = true;
        ESP_LOGI(TAG, "Subscribed to voltage");
    }
    return ret;
}

static esp_err_t bap_subscribe_fan_rpm(void) {
    if( subscribed_fan_rpm) {
        ESP_LOGW(TAG, "Already subscribed to fan RPM, skipping");
        return ESP_OK;
    }

    esp_err_t ret = bap_client_subscribe("fan_speed");
    if (ret == ESP_OK) {
        subscribed_fan_rpm = true;
        ESP_LOGI(TAG, "Subscribed to fan RPM");
    }
    return ret;
}

static esp_err_t bap_subscribe_fan_speed_percent(void) {
    if (subscribed_fan_speed_percent) {
        ESP_LOGW(TAG, "Already subscribed to fan speed percent, skipping");
        return ESP_OK;
    }

    esp_err_t ret = bap_client_subscribe("fan_speed_percent");
    if (ret == ESP_OK) {
        subscribed_fan_speed_percent = true;
        ESP_LOGI(TAG, "Subscribed to fan speed percent");
    }
    return ret;
}

static esp_err_t bap_request_system_info(void) {
    if (system_info_requested) {
        ESP_LOGW(TAG, "System info already requested, skipping");
        return ESP_OK;
    }
    
    esp_err_t ret = bap_client_request("systemInfo");
    if (ret == ESP_OK) {
        system_info_requested = true;
        ESP_LOGI(TAG, "Sent system info request");
    }
    return ret;
}

static void transport_send_task(void *pvParameters) {
    if (subscriptions_sent) {
        ESP_LOGI(TAG, "Subscriptions already sent, exiting task");
        vTaskDelete(NULL);
        return;
    }
    
    // Wait 7 seconds before sending subscription
    ESP_LOGI(TAG, "Waiting 7 seconds before sending subscription...");
    vTaskDelay(pdMS_TO_TICKS(7000));
    
    bap_subscribe_hashrate();
    vTaskDelay(pdMS_TO_TICKS(100));  // Wait a bit before next subscription
    bap_subscribe_temperature();
    vTaskDelay(pdMS_TO_TICKS(100));  // Wait a bit before next subscription
    bap_subscribe_power();
    vTaskDelay(pdMS_TO_TICKS(100));  // Wait a bit before next subscription
    bap_subscribe_voltage();
    vTaskDelay(pdMS_TO_TICKS(100));  // Wait a bit before next subscription
    bap_subscribe_fan_rpm();
    vTaskDelay(pdMS_TO_TICKS(100));  // Wait a bit before next subscription
    bap_subscribe_fan_speed_percent();
    vTaskDelay(pdMS_TO_TICKS(100));  // Wait a bit before next subscription
    bap_subscribe_shares();
    vTaskDelay(pdMS_TO_TICKS(100));  // Wait a bit before next subscription
    bap_subscribe_best_difficulty();
    vTaskDelay(pdMS_TO_TICKS(100));  // Wait a bit before next subscription
    bap_subscribe_block_height();
    vTaskDelay(pdMS_TO_TICKS(100));  // Wait a bit
    bap_request_system_info();
    
    subscriptions_sent = true;
    ESP_LOGI(TAG, "All subscriptions sent, task exiting");
    vTaskDelete(NULL);
}

static void transport_receive_task(void *pvParameters) {
    static uint8_t rx_buffer[256];
    static char line_buffer[1024];
    size_t line_len = 0;
    
    esp_err_t wdt_ret = esp_task_wdt_add(NULL);
    if (wdt_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to add BAP receive task to watchdog: %s", esp_err_to_name(wdt_ret));
    }
    
    while (1) {
        if (wdt_ret == ESP_OK) {
            esp_task_wdt_reset();
        }
        
        int len = bap_transport_read(rx_buffer, sizeof(rx_buffer), 100);
        if (len > 0) {
            for (int i = 0; i < len; ++i) {
                char ch = (char)rx_buffer[i];

                if (line_len < sizeof(line_buffer) - 1) {
                    line_buffer[line_len++] = ch;
                    line_buffer[line_len] = '\0';
                } else {
                    line_len = 0;
                    continue;
                }

                if (line_len >= 2 &&
                    line_buffer[line_len - 2] == '\r' &&
                    line_buffer[line_len - 1] == '\n') {
                    line_buffer[line_len - 2] = '\0';
                    if (line_buffer[0] == '$') {
                        last_response_time = xTaskGetTickCount();
                        bap_parse_and_handle_message(line_buffer);
                    }
                    line_len = 0;
                } else if (ch == '\n') {
                    line_buffer[line_len - 1] = '\0';
                    if (line_buffer[0] == '$') {
                        last_response_time = xTaskGetTickCount();
                        bap_parse_and_handle_message(line_buffer);
                    }
                    line_len = 0;
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void connection_monitor_task(void *pvParameters) {
    esp_err_t wdt_ret = esp_task_wdt_add(NULL);
    if (wdt_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to add connection monitor task to watchdog: %s", esp_err_to_name(wdt_ret));
    }
    
    for (int i = 0; i < 150; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
        if (wdt_ret == ESP_OK) {
            esp_task_wdt_reset();
        }
    }
    
    while (1) {
        if (wdt_ret == ESP_OK) {
            esp_task_wdt_reset();
        }
        
        uint32_t current_time = xTaskGetTickCount();
        
        // Check if we haven't received any response for 12 seconds
        if (last_response_time > 0 && (current_time - last_response_time) > pdMS_TO_TICKS(12000)) {
            ESP_LOGW(TAG, "No response for 12s, resetting connection state");
            
            bap_client_reset_connection_state();
            
            BaseType_t task_ret = xTaskCreate(transport_send_task, "bap_tx_retry", 4096, NULL, 5, NULL);
            if (task_ret != pdPASS) {
                ESP_LOGE(TAG, "Failed to create retry BAP send task");
            } else {
                ESP_LOGI(TAG, "Created retry task to re-establish connection");
            }
        }
        
        // Check every 10 seconds, but reset watchdog more frequently
        for (int i = 0; i < 100; i++) {
            vTaskDelay(pdMS_TO_TICKS(100));
            if (wdt_ret == ESP_OK) {
                esp_task_wdt_reset();
            }
        }
    }
}

void bap_client_suspend(void) {
    ESP_LOGW(TAG, "╔═══════════════════════════════════════╗");
    ESP_LOGW(TAG, "║  SUSPENDING BAP CLIENT TASKS         ║");
    ESP_LOGW(TAG, "╚═══════════════════════════════════════╝");

    if (transport_receive_task_handle != NULL) {
        // Remove from watchdog before suspending
        esp_err_t ret = esp_task_wdt_delete(transport_receive_task_handle);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to remove bap_rx from watchdog: %s", esp_err_to_name(ret));
        }
        vTaskSuspend(transport_receive_task_handle);
        ESP_LOGW(TAG, "✓ BAP receive task suspended");
    } else {
        ESP_LOGW(TAG, "✗ BAP receive task handle is NULL");
    }

    if (connection_monitor_task_handle != NULL) {
        // Remove from watchdog before suspending
        esp_err_t ret = esp_task_wdt_delete(connection_monitor_task_handle);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to remove connection_monitor from watchdog: %s", esp_err_to_name(ret));
        }
        vTaskSuspend(connection_monitor_task_handle);
        ESP_LOGW(TAG, "✓ Connection monitor task suspended");
    } else {
        ESP_LOGW(TAG, "✗ Connection monitor task handle is NULL");
    }

    ESP_LOGW(TAG, "═══════════════════════════════════════");
    ESP_LOGW(TAG, "BAP tasks suspended - no more updates");
    ESP_LOGW(TAG, "═══════════════════════════════════════");
}

void bap_client_resume(void) {
    ESP_LOGW(TAG, "╔═══════════════════════════════════════╗");
    ESP_LOGW(TAG, "║  RESUMING BAP CLIENT TASKS           ║");
    ESP_LOGW(TAG, "╚═══════════════════════════════════════╝");

    if (transport_receive_task_handle != NULL) {
        vTaskResume(transport_receive_task_handle);
        // Re-add to watchdog after resuming
        esp_err_t ret = esp_task_wdt_add(transport_receive_task_handle);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to re-add bap_rx to watchdog: %s", esp_err_to_name(ret));
        }
        ESP_LOGW(TAG, "✓ BAP receive task resumed");
    } else {
        ESP_LOGW(TAG, "✗ BAP receive task handle is NULL");
    }

    if (connection_monitor_task_handle != NULL) {
        vTaskResume(connection_monitor_task_handle);
        // Re-add to watchdog after resuming
        esp_err_t ret = esp_task_wdt_add(connection_monitor_task_handle);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to re-add connection_monitor to watchdog: %s", esp_err_to_name(ret));
        }
        ESP_LOGW(TAG, "✓ Connection monitor task resumed");
    } else {
        ESP_LOGW(TAG, "✗ Connection monitor task handle is NULL");
    }

    ESP_LOGW(TAG, "═══════════════════════════════════════");
    ESP_LOGW(TAG, "BAP tasks resumed - updates restarted");
    ESP_LOGW(TAG, "═══════════════════════════════════════");
}
