#include "bap_transport.h"

#include "usb_bridge.h"

#define BAP_TRANSPORT_BUF_SIZE 2048

esp_err_t bap_transport_init(void)
{
    return usb_bridge_init();
}

esp_err_t bap_transport_write(const char *data, size_t length)
{
    return usb_bridge_cdc_write((const uint8_t *)data, length);
}

int bap_transport_read(uint8_t *buffer, size_t buffer_size, uint32_t timeout_ms)
{
    return usb_bridge_cdc_read(buffer, buffer_size, timeout_ms);
}

esp_err_t bap_transport_flush(void)
{
    return usb_bridge_cdc_flush(0);
}

esp_err_t bap_transport_deinit(void)
{
    return ESP_OK;
}

size_t bap_transport_get_buffer_size(void)
{
    return BAP_TRANSPORT_BUF_SIZE;
}
