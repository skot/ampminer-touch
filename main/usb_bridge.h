#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t usb_bridge_init(void);

bool usb_bridge_cdc_connected(void);
int usb_bridge_cdc_read(uint8_t *buffer, size_t buffer_size, uint32_t timeout_ms);
esp_err_t usb_bridge_cdc_write(const uint8_t *data, size_t length);
esp_err_t usb_bridge_cdc_flush(uint32_t timeout_ticks);

esp_err_t usb_bridge_send_network_packet(void *buffer, uint16_t len, void *eb);
esp_err_t usb_bridge_wifi_recv_callback(void *buffer, uint16_t len, void *ctx);
void usb_bridge_wifi_buffer_free(void *buffer, void *ctx);
void usb_bridge_set_network_link(bool connected);

#ifdef __cplusplus
}
#endif
