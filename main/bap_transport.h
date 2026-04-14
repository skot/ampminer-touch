#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t bap_transport_init(void);
esp_err_t bap_transport_write(const char *data, size_t length);
int bap_transport_read(uint8_t *buffer, size_t buffer_size, uint32_t timeout_ms);
esp_err_t bap_transport_flush(void);
esp_err_t bap_transport_deinit(void);
size_t bap_transport_get_buffer_size(void);

#ifdef __cplusplus
}
#endif
