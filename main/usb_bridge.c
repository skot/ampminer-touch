#include "usb_bridge.h"

#include <string.h>

#include "esp_log.h"
#include "esp_mac.h"
#include "esp_private/wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"
#include "sdkconfig.h"
#include "tinyusb.h"
#include "tinyusb_net.h"
#include "tusb.h"
#include "tusb_cdc_acm.h"

static const char *TAG = "USB_BRIDGE";

#define USB_BRIDGE_CDC_PORT TINYUSB_CDC_ACM_0
#define USB_BRIDGE_RX_STREAM_SIZE 2048
#define USB_BRIDGE_RX_TRIGGER_LEVEL 1

DRAM_ATTR uint8_t tud_network_mac_address[6] = {0x02, 0x02, 0x84, 0x6A, 0x96, 0x00};

static bool s_usb_bridge_initialized = false;
static bool s_cdc_host_connected = false;
static bool s_wifi_connected = false;
static StreamBufferHandle_t s_cdc_rx_stream;
static uint8_t s_cdc_rx_storage[USB_BRIDGE_RX_STREAM_SIZE];
static StaticStreamBuffer_t s_cdc_rx_stream_struct;

static void usb_bridge_cdc_rx_callback(int itf, cdcacm_event_t *event);
static void usb_bridge_cdc_line_state_changed_callback(int itf, cdcacm_event_t *event);
static void usb_bridge_net_init_callback(void *ctx);

void tud_mount_cb(void)
{
    ESP_LOGI(TAG, "TinyUSB mounted");
    usb_bridge_set_network_link(s_wifi_connected);
}

void tud_umount_cb(void)
{
    ESP_LOGW(TAG, "TinyUSB unmounted");
}

void tud_suspend_cb(bool remote_wakeup_en)
{
    ESP_LOGW(TAG, "TinyUSB suspended, remote_wakeup=%d", remote_wakeup_en);
}

void tud_resume_cb(void)
{
    ESP_LOGI(TAG, "TinyUSB resumed");
    usb_bridge_set_network_link(s_wifi_connected);
}

static void usb_bridge_cdc_rx_callback(int itf, cdcacm_event_t *event)
{
    (void)itf;
    (void)event;

    uint8_t tmp[CONFIG_TINYUSB_CDC_RX_BUFSIZE];
    size_t rx_size = 0;

    if (!s_cdc_rx_stream) {
        return;
    }

    while (tinyusb_cdcacm_read(USB_BRIDGE_CDC_PORT, tmp, sizeof(tmp), &rx_size) == ESP_OK && rx_size > 0) {
        size_t written = xStreamBufferSend(s_cdc_rx_stream, tmp, rx_size, 0);
        if (written < rx_size) {
            ESP_LOGW(TAG, "CDC RX overflow, dropped %u bytes", (unsigned int)(rx_size - written));
        }
    }
}

static void usb_bridge_cdc_line_state_changed_callback(int itf, cdcacm_event_t *event)
{
    (void)itf;
    if (!event) {
        return;
    }

    s_cdc_host_connected = event->line_state_changed_data.dtr;
    ESP_LOGI(TAG, "CDC line state changed: DTR=%d RTS=%d",
             event->line_state_changed_data.dtr,
             event->line_state_changed_data.rts);
}

static void usb_bridge_net_init_callback(void *ctx)
{
    (void)ctx;
    ESP_LOGI(TAG, "ECM network init callback, wifi_connected=%d, tud_ready=%d",
             s_wifi_connected, tud_ready());
    usb_bridge_set_network_link(s_wifi_connected);
}

esp_err_t usb_bridge_init(void)
{
    if (s_usb_bridge_initialized) {
        return ESP_OK;
    }

    s_cdc_rx_stream = xStreamBufferCreateStatic(
        sizeof(s_cdc_rx_storage),
        USB_BRIDGE_RX_TRIGGER_LEVEL,
        s_cdc_rx_storage,
        &s_cdc_rx_stream_struct);
    if (!s_cdc_rx_stream) {
        return ESP_ERR_NO_MEM;
    }

    uint8_t mac_addr[6] = {0};
    ESP_ERROR_CHECK(esp_read_mac(mac_addr, ESP_MAC_WIFI_STA));
    memcpy(tud_network_mac_address, mac_addr, sizeof(mac_addr));

    tinyusb_config_t tusb_cfg = {
        .external_phy = false,
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    tinyusb_net_config_t tusb_net_cfg = {
        .on_recv_callback = usb_bridge_wifi_recv_callback,
        .free_tx_buffer = usb_bridge_wifi_buffer_free,
        .on_init_callback = usb_bridge_net_init_callback,
    };
    memcpy(tusb_net_cfg.mac_addr, mac_addr, sizeof(mac_addr));
    ESP_ERROR_CHECK(tinyusb_net_init(TINYUSB_USBDEV_0, &tusb_net_cfg));

    tinyusb_config_cdcacm_t cdc_cfg = {
        .usb_dev = TINYUSB_USBDEV_0,
        .cdc_port = USB_BRIDGE_CDC_PORT,
        .rx_unread_buf_sz = 128,
        .callback_rx = usb_bridge_cdc_rx_callback,
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = usb_bridge_cdc_line_state_changed_callback,
        .callback_line_coding_changed = NULL,
    };
    ESP_ERROR_CHECK(tusb_cdc_acm_init(&cdc_cfg));

    s_usb_bridge_initialized = true;

    ESP_LOGI(TAG, "USB bridge initialized with ECM + CDC");
    ESP_LOGI(TAG, "USB MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             mac_addr[0], mac_addr[1], mac_addr[2],
             mac_addr[3], mac_addr[4], mac_addr[5]);

    return ESP_OK;
}

bool usb_bridge_cdc_connected(void)
{
    return s_cdc_host_connected;
}

int usb_bridge_cdc_read(uint8_t *buffer, size_t buffer_size, uint32_t timeout_ms)
{
    if (!buffer || buffer_size == 0 || !s_cdc_rx_stream) {
        return -1;
    }

    size_t received = xStreamBufferReceive(
        s_cdc_rx_stream,
        buffer,
        buffer_size,
        pdMS_TO_TICKS(timeout_ms));

    return (int)received;
}

esp_err_t usb_bridge_cdc_write(const uint8_t *data, size_t length)
{
    if (!data || length == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t queued = tinyusb_cdcacm_write_queue(USB_BRIDGE_CDC_PORT, data, length);
    if (queued != length) {
        return ESP_ERR_INVALID_SIZE;
    }

    return usb_bridge_cdc_flush(pdMS_TO_TICKS(50));
}

esp_err_t usb_bridge_cdc_flush(uint32_t timeout_ticks)
{
    return tinyusb_cdcacm_write_flush(USB_BRIDGE_CDC_PORT, timeout_ticks);
}

esp_err_t usb_bridge_send_network_packet(void *buffer, uint16_t len, void *eb)
{
    if (tinyusb_net_send(buffer, len, eb) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send Wi-Fi packet to USB host, dropping frame len=%u", len);
        esp_wifi_internal_free_rx_buffer(eb);
    }
    return ESP_OK;
}

esp_err_t usb_bridge_wifi_recv_callback(void *buffer, uint16_t len, void *ctx)
{
    (void)ctx;
    if (s_wifi_connected) {
        esp_wifi_internal_tx(ESP_IF_WIFI_STA, buffer, len);
    }
    return ESP_OK;
}

void usb_bridge_wifi_buffer_free(void *buffer, void *ctx)
{
    (void)ctx;
    esp_wifi_internal_free_rx_buffer(buffer);
}

void usb_bridge_set_network_link(bool connected)
{
    s_wifi_connected = connected;
    ESP_LOGI(TAG, "Setting ECM link %s, tud_ready=%d", connected ? "UP" : "DOWN", tud_ready());
    if (tud_ready()) {
        tud_network_link_state(0, connected);
    }
}
