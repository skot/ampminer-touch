#ifndef SETTINGS_H
#define SETTINGS_H

#include "lvgl.h"

typedef enum {
    PERFORMANCE_LOW = 0,
    PERFORMANCE_MEDIUM,
    PERFORMANCE_HIGH
} performance_mode_t;

typedef struct {
    performance_mode_t performance_mode;
    bool auto_fan_control;
    int fan_speed_percent;  // 0-100%
    int brightness_percent; // 0-100%
} settings_info_t;

void settings_screen_create(void);
void settings_screen_destroy(void);
lv_obj_t* settings_get_screen(void);
void settings_update_info(const settings_info_t* info);
int settings_get_fan_speed_percent(void);
float settings_get_asic_voltage_mv(void);
void settings_wifi_clear_networks(void);
void settings_wifi_add_network(const char *ssid);
void settings_wifi_update_status(const char *status);
void settings_wifi_finish_scan(const char *status);
void settings_update_miner_ip(const char *ip);

void settings_auto_fan_toggled(lv_event_t * e);
void settings_fan_slider_changed(lv_event_t * e);
void settings_fan_save_clicked(lv_event_t * e);
void settings_wifi_scan_clicked(lv_event_t * e);
void settings_wifi_connect_clicked(lv_event_t * e);
void settings_home_clicked(lv_event_t * e);
void settings_block_clicked(lv_event_t * e);
void settings_night_clicked(lv_event_t * e);

#endif // SETTINGS_H
