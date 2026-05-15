#include "settings.h"
#include "home.h"
#include "night.h"
#include "block.h"
#include "stdio.h"
#include "string.h"
#include "custom_fonts.h"
#include "bap.h"

static lv_obj_t *settings_screen = NULL;
static lv_obj_t *miner_ip_value_label = NULL;
static lv_obj_t *auto_fan_checkbox = NULL;
static lv_obj_t *fan_slider = NULL;
static lv_obj_t *fan_value_label = NULL;
static lv_obj_t *fan_save_btn = NULL;
static lv_obj_t *wifi_dropdown = NULL;
static lv_obj_t *wifi_password_ta = NULL;
static lv_obj_t *wifi_status_label = NULL;
#if LV_USE_KEYBOARD
static lv_obj_t *wifi_keyboard = NULL;
#endif
static lv_obj_t *sys_overlay = NULL;
static int diag_counter = 0;

static settings_info_t current_settings = {
    .performance_mode = PERFORMANCE_MEDIUM,
    .auto_fan_control = true,
    .fan_speed_percent = 50,
    .brightness_percent = 100};
static int applied_fan_speed_percent = 50;
static float applied_asic_voltage_mv = 1200.0f;

static char current_miner_ip[48] = "-";
static char wifi_network_options[512] = "Tap Scan";
static int wifi_network_count = 0;

#define SETTINGS_BOTTOM_NAV_HEIGHT 64
#define SETTINGS_WIFI_KEYBOARD_HEIGHT 170

static float settings_voltage_for_mode(void)
{
    return 1200.0f;
}

static lv_obj_t *create_settings_button(lv_obj_t *parent, const char *text, lv_event_cb_t event_cb, bool active)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 170, 48);
    lv_obj_set_style_bg_color(btn, active ? COLOR_ACCENT : COLOR_CARD_BG, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, active ? 0 : 2, 0);
    lv_obj_set_style_border_color(btn, COLOR_ACCENT, 0);
    lv_obj_set_style_border_opa(btn, active ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);

    lv_obj_set_style_bg_color(btn, COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_20, LV_STATE_PRESSED);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, active ? COLOR_TEXT_ON_ACCENT : COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_center(label);

    if (event_cb)
    {
        lv_obj_add_event_cb(btn, event_cb, LV_EVENT_CLICKED, NULL);
    }

    return btn;
}

static lv_obj_t *create_bottom_nav_btn(lv_obj_t *parent, const char *symbol, lv_event_cb_t event_cb, bool active)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 56, 46);
    lv_obj_set_style_bg_color(btn, active ? COLOR_ACCENT : COLOR_CARD_BG, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, active ? 0 : 2, 0);
    lv_obj_set_style_border_color(btn, COLOR_ACCENT, 0);
    lv_obj_set_style_border_opa(btn, active ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, symbol);
    lv_obj_set_style_text_color(label, active ? COLOR_TEXT_ON_ACCENT : COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
    lv_obj_center(label);

    if (event_cb)
    {
        lv_obj_add_event_cb(btn, event_cb, LV_EVENT_CLICKED, NULL);
    }

    return btn;
}

static lv_obj_t *create_bottom_nav_btn_img(lv_obj_t *parent, const lv_img_dsc_t *img_dsc, lv_event_cb_t event_cb, bool active)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 56, 46);
    lv_obj_set_style_bg_color(btn, active ? COLOR_ACCENT : COLOR_CARD_BG, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, active ? 0 : 2, 0);
    lv_obj_set_style_border_color(btn, COLOR_ACCENT, 0);
    lv_obj_set_style_border_opa(btn, active ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);

    lv_obj_t *img = lv_img_create(btn);
    lv_img_set_src(img, img_dsc);
    lv_obj_set_style_img_recolor(img, active ? COLOR_TEXT_ON_ACCENT : COLOR_ACCENT, 0);
    lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
    lv_obj_center(img);

    if (event_cb)
    {
        lv_obj_add_event_cb(btn, event_cb, LV_EVENT_CLICKED, NULL);
    }

    return btn;
}

static void update_fan_controls(void)
{
    if (!fan_slider || !fan_value_label)
        return;

    if (current_settings.auto_fan_control)
    {
        lv_obj_add_flag(fan_slider, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(fan_value_label, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_clear_flag(fan_slider, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(fan_value_label, LV_OBJ_FLAG_HIDDEN);

        lv_slider_set_value(fan_slider, current_settings.fan_speed_percent, LV_ANIM_OFF);
        char fan_text[16];
        snprintf(fan_text, sizeof(fan_text), "%d%%", current_settings.fan_speed_percent);
        lv_label_set_text(fan_value_label, fan_text);
    }
}

static bool wifi_option_matches(const char *ssid, const char *option, size_t option_len)
{
    return strlen(ssid) == option_len && strncmp(ssid, option, option_len) == 0;
}

static bool wifi_options_contains(const char *ssid)
{
    const char *cursor = wifi_network_options;

    while (*cursor) {
        const char *line_end = strchr(cursor, '\n');
        size_t option_len = line_end ? (size_t)(line_end - cursor) : strlen(cursor);

        if (wifi_option_matches(ssid, cursor, option_len)) {
            return true;
        }

        if (!line_end) {
            break;
        }
        cursor = line_end + 1;
    }

    return false;
}

void settings_wifi_clear_networks(void)
{
    wifi_network_count = 0;
    snprintf(wifi_network_options, sizeof(wifi_network_options), "Scanning...");

    if (wifi_dropdown) {
        lv_dropdown_set_options(wifi_dropdown, wifi_network_options);
        lv_dropdown_set_selected(wifi_dropdown, 0);
    }
}

void settings_wifi_add_network(const char *ssid)
{
    if (!ssid || ssid[0] == '\0' || wifi_options_contains(ssid)) {
        return;
    }

    if (wifi_network_count == 0) {
        wifi_network_options[0] = '\0';
    }

    size_t used = strlen(wifi_network_options);
    int written = snprintf(wifi_network_options + used,
                           sizeof(wifi_network_options) - used,
                           "%s%s",
                           used > 0 ? "\n" : "",
                           ssid);
    if (written < 0 || (size_t)written >= sizeof(wifi_network_options) - used) {
        if (wifi_status_label) {
            lv_label_set_text(wifi_status_label, "Network list full");
        }
        return;
    }

    wifi_network_count++;

    if (wifi_dropdown) {
        lv_dropdown_set_options(wifi_dropdown, wifi_network_options);
        lv_dropdown_set_selected(wifi_dropdown, 0);
    }

    if (wifi_status_label) {
        char status[48];
        snprintf(status, sizeof(status), "%d network%s found",
                 wifi_network_count,
                 wifi_network_count == 1 ? "" : "s");
        lv_label_set_text(wifi_status_label, status);
    }
}

void settings_wifi_update_status(const char *status)
{
    if (!status) {
        return;
    }

    if (wifi_status_label) {
        lv_label_set_text(wifi_status_label, status);
    }
}

void settings_wifi_finish_scan(const char *status)
{
    if (wifi_network_count == 0 && strcmp(wifi_network_options, "Scanning...") == 0) {
        snprintf(wifi_network_options, sizeof(wifi_network_options), "No networks found");
        if (wifi_dropdown) {
            lv_dropdown_set_options(wifi_dropdown, wifi_network_options);
            lv_dropdown_set_selected(wifi_dropdown, 0);
        }
    }

    settings_wifi_update_status(status && status[0] ? status : "Scan complete");
}

void settings_update_miner_ip(const char *ip)
{
    if (!ip || ip[0] == '\0') {
        ip = "-";
    }

    strncpy(current_miner_ip, ip, sizeof(current_miner_ip) - 1);
    current_miner_ip[sizeof(current_miner_ip) - 1] = '\0';

    if (miner_ip_value_label) {
        lv_label_set_text(miner_ip_value_label, current_miner_ip);
    }
}

#if LV_USE_KEYBOARD
static void wifi_password_ta_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (!wifi_keyboard) {
        return;
    }

    if (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED) {
        lv_keyboard_set_textarea(wifi_keyboard, wifi_password_ta);
        lv_obj_set_size(wifi_keyboard, SCREEN_WIDTH, SETTINGS_WIFI_KEYBOARD_HEIGHT);
        lv_obj_align(wifi_keyboard, LV_ALIGN_BOTTOM_MID, 0, -SETTINGS_BOTTOM_NAV_HEIGHT);
        lv_obj_clear_flag(wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(wifi_keyboard);
    } else if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL || code == LV_EVENT_DEFOCUSED) {
        lv_obj_add_flag(wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}
#endif

static void decode_sys_info(char *output, size_t output_size)
{
    static const uint8_t encoded_data[] = {
        38, 39, 52, 39, 46, 45, 50, 39, 38, 98,
        32, 59, 98, 21, 35, 44, 54, 1, 46, 55, 39
    };
    const uint8_t key = 0x42;
    size_t len = sizeof(encoded_data);

    for (size_t i = 0; i < len && i < output_size - 1; i++) {
        output[i] = encoded_data[i] ^ key;
    }
    output[len < output_size ? len : output_size - 1] = '\0';
}

static void cleanup_system_overlay(lv_event_t *e)
{
    if (sys_overlay) {
        lv_obj_del(sys_overlay);
        sys_overlay = NULL;
    }
    diag_counter = 0;
}

static void create_system_overlay(void)
{
    if (sys_overlay) {
        return;
    }

    sys_overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(sys_overlay, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(sys_overlay, 0, 0);
    lv_obj_set_style_bg_color(sys_overlay, COLOR_BACKGROUND, 0);
    lv_obj_set_style_bg_opa(sys_overlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(sys_overlay, 0, 0);
    lv_obj_clear_flag(sys_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(sys_overlay, cleanup_system_overlay, LV_EVENT_CLICKED, NULL);

    lv_obj_t *dialog = lv_obj_create(sys_overlay);
    lv_obj_set_size(dialog, 400, 200);
    lv_obj_center(dialog);
    lv_obj_set_style_bg_color(dialog, COLOR_CARD_BG, 0);
    lv_obj_set_style_bg_opa(dialog, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dialog, 2, 0);
    lv_obj_set_style_border_color(dialog, COLOR_ACCENT, 0);
    lv_obj_set_style_border_opa(dialog, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(dialog, 14, 0);
    lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);

    char display_buffer[64];
    decode_sys_info(display_buffer, sizeof(display_buffer));

    lv_obj_t *label = lv_label_create(dialog);
    lv_label_set_text(label, display_buffer);
    lv_obj_set_style_text_color(label, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_22, 0);
    lv_obj_center(label);
}

static void settings_diagnostics_handler(lv_event_t *e)
{
    diag_counter++;

    if (diag_counter >= 3) {
        create_system_overlay();
        diag_counter = 0;
    }
}

void settings_screen_create(void)
{
    if (settings_screen != NULL)
    {
        return;
    }

    settings_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(settings_screen, COLOR_BACKGROUND, 0);
    lv_obj_set_style_bg_opa(settings_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(settings_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(settings_screen, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *main_cont = lv_obj_create(settings_screen);
    lv_obj_set_size(main_cont, SCREEN_WIDTH - 60, SCREEN_HEIGHT - 100);
    lv_obj_align(main_cont, LV_ALIGN_TOP_MID, 0, 16);
    lv_obj_set_style_bg_color(main_cont, COLOR_CARD_BG, 0);
    lv_obj_set_style_bg_opa(main_cont, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(main_cont, 1, 0);
    lv_obj_set_style_border_color(main_cont, COLOR_BORDER, 0);
    lv_obj_set_style_border_opa(main_cont, LV_OPA_50, 0);
    lv_obj_set_style_radius(main_cont, 14, 0);
    lv_obj_set_style_pad_all(main_cont, 16, 0);
    lv_obj_add_flag(main_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(main_cont, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_dir(main_cont, LV_DIR_VER);
    lv_obj_set_style_pad_bottom(main_cont, 80, 0);

    lv_obj_t *title_label = lv_label_create(main_cont);
    lv_label_set_text(title_label, "SETTINGS");
    lv_obj_set_style_text_color(title_label, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_28, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 6);

    lv_obj_t *ip_section = lv_obj_create(main_cont);
    lv_obj_set_size(ip_section, 680, 70);
    lv_obj_align(ip_section, LV_ALIGN_TOP_MID, 0, 46);
    lv_obj_set_style_bg_opa(ip_section, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ip_section, 0, 0);
    lv_obj_set_style_pad_all(ip_section, 10, 0);
    lv_obj_clear_flag(ip_section, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ip_title = lv_label_create(ip_section);
    lv_label_set_text(ip_title, "Miner IP:");
    lv_obj_set_style_text_color(ip_title, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(ip_title, &lv_font_montserrat_18, 0);
    lv_obj_align(ip_title, LV_ALIGN_TOP_LEFT, 0, 0);

    miner_ip_value_label = lv_label_create(ip_section);
    lv_label_set_text(miner_ip_value_label, current_miner_ip);
    lv_obj_set_style_text_color(miner_ip_value_label, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(miner_ip_value_label, &lv_font_montserrat_22, 0);
    lv_obj_align(miner_ip_value_label, LV_ALIGN_TOP_LEFT, 140, -2);

    lv_obj_t *fan_section = lv_obj_create(main_cont);
    lv_obj_set_size(fan_section, 680, 200);
    lv_obj_align(fan_section, LV_ALIGN_TOP_MID, 0, 120);
    lv_obj_set_style_bg_opa(fan_section, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(fan_section, 0, 0);
    lv_obj_set_style_pad_all(fan_section, 10, 0);
    lv_obj_clear_flag(fan_section, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *fan_title = lv_label_create(fan_section);
    lv_label_set_text(fan_title, "Fan Control:");
    lv_obj_set_style_text_color(fan_title, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(fan_title, &lv_font_montserrat_18, 0);
    lv_obj_align(fan_title, LV_ALIGN_TOP_LEFT, 0, 0);

    auto_fan_checkbox = lv_checkbox_create(fan_section);
    lv_checkbox_set_text(auto_fan_checkbox, "Automatic Fan Control");
    lv_obj_set_style_text_color(auto_fan_checkbox, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(auto_fan_checkbox, &lv_font_montserrat_16, 0);
    lv_obj_align(auto_fan_checkbox, LV_ALIGN_TOP_LEFT, 0, 35);
    lv_obj_add_event_cb(auto_fan_checkbox, settings_auto_fan_toggled, LV_EVENT_VALUE_CHANGED, NULL);

    if (current_settings.auto_fan_control)
    {
        lv_obj_add_state(auto_fan_checkbox, LV_STATE_CHECKED);
    }

    fan_slider = lv_slider_create(fan_section);
    lv_obj_set_size(fan_slider, 420, 20);
    lv_obj_align(fan_slider, LV_ALIGN_TOP_LEFT, 0, 70);
    lv_slider_set_range(fan_slider, 0, 100);
    lv_slider_set_value(fan_slider, current_settings.fan_speed_percent, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(fan_slider, COLOR_CARD_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_color(fan_slider, COLOR_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(fan_slider, COLOR_ACCENT, LV_PART_KNOB);
    lv_obj_add_event_cb(fan_slider, settings_fan_slider_changed, LV_EVENT_VALUE_CHANGED, NULL);

    fan_value_label = lv_label_create(fan_section);
    char fan_text[16];
    snprintf(fan_text, sizeof(fan_text), "%d%%", current_settings.fan_speed_percent);
    lv_label_set_text(fan_value_label, fan_text);
    lv_obj_set_style_text_color(fan_value_label, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(fan_value_label, &lv_font_montserrat_16, 0);
    lv_obj_align(fan_value_label, LV_ALIGN_TOP_LEFT, 450, 65);

    fan_save_btn = create_settings_button(fan_section, "SAVE FAN SETTINGS", settings_fan_save_clicked, false);
    lv_obj_set_size(fan_save_btn, 220, 36);
    lv_obj_align(fan_save_btn, LV_ALIGN_TOP_LEFT, 0, 115);

    update_fan_controls();

    lv_obj_t *wifi_section = lv_obj_create(main_cont);
    lv_obj_set_size(wifi_section, 680, 180);
    lv_obj_align(wifi_section, LV_ALIGN_TOP_MID, 0, 330);
    lv_obj_set_style_bg_opa(wifi_section, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wifi_section, 0, 0);
    lv_obj_set_style_pad_all(wifi_section, 10, 0);
    lv_obj_clear_flag(wifi_section, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *wifi_title = lv_label_create(wifi_section);
    lv_label_set_text(wifi_title, "Control Board Wi-Fi:");
    lv_obj_set_style_text_color(wifi_title, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(wifi_title, &lv_font_montserrat_18, 0);
    lv_obj_align(wifi_title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *wifi_scan_btn = create_settings_button(wifi_section, "SCAN", settings_wifi_scan_clicked, false);
    lv_obj_set_size(wifi_scan_btn, 110, 34);
    lv_obj_align(wifi_scan_btn, LV_ALIGN_TOP_RIGHT, 0, -4);

    wifi_dropdown = lv_dropdown_create(wifi_section);
    lv_obj_set_size(wifi_dropdown, 410, 34);
    lv_obj_align(wifi_dropdown, LV_ALIGN_TOP_LEFT, 0, 38);
    lv_dropdown_set_options(wifi_dropdown, wifi_network_options);
    lv_obj_set_style_bg_color(wifi_dropdown, COLOR_CARD_BG, 0);
    lv_obj_set_style_bg_opa(wifi_dropdown, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(wifi_dropdown, 1, 0);
    lv_obj_set_style_border_color(wifi_dropdown, COLOR_ACCENT, 0);
    lv_obj_set_style_border_opa(wifi_dropdown, LV_OPA_50, 0);
    lv_obj_set_style_radius(wifi_dropdown, 8, 0);
    lv_obj_set_style_text_color(wifi_dropdown, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(wifi_dropdown, &lv_font_montserrat_16, 0);

    wifi_password_ta = lv_textarea_create(wifi_section);
    lv_obj_set_size(wifi_password_ta, 410, 38);
    lv_obj_align(wifi_password_ta, LV_ALIGN_TOP_LEFT, 0, 86);
    lv_textarea_set_one_line(wifi_password_ta, true);
    lv_textarea_set_password_mode(wifi_password_ta, true);
    lv_textarea_set_max_length(wifi_password_ta, 63);
    lv_textarea_set_placeholder_text(wifi_password_ta, "Password");
    lv_obj_set_style_bg_color(wifi_password_ta, COLOR_CARD_BG, 0);
    lv_obj_set_style_bg_opa(wifi_password_ta, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(wifi_password_ta, 1, 0);
    lv_obj_set_style_border_color(wifi_password_ta, COLOR_ACCENT, 0);
    lv_obj_set_style_border_opa(wifi_password_ta, LV_OPA_50, 0);
    lv_obj_set_style_radius(wifi_password_ta, 8, 0);
    lv_obj_set_style_text_color(wifi_password_ta, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(wifi_password_ta, &lv_font_montserrat_16, 0);
#if LV_USE_KEYBOARD
    lv_obj_add_event_cb(wifi_password_ta, wifi_password_ta_event_cb, LV_EVENT_ALL, NULL);
#endif

    lv_obj_t *wifi_connect_btn = create_settings_button(wifi_section, "CONNECT", settings_wifi_connect_clicked, false);
    lv_obj_set_size(wifi_connect_btn, 150, 38);
    lv_obj_align(wifi_connect_btn, LV_ALIGN_TOP_LEFT, 430, 86);

    wifi_status_label = lv_label_create(wifi_section);
    lv_label_set_text(wifi_status_label, "Ready");
    lv_obj_set_style_text_color(wifi_status_label, COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(wifi_status_label, &lv_font_montserrat_16, 0);
    lv_obj_set_width(wifi_status_label, 620);
    lv_obj_align(wifi_status_label, LV_ALIGN_TOP_LEFT, 0, 140);

#if LV_USE_KEYBOARD
    wifi_keyboard = lv_keyboard_create(settings_screen);
    lv_obj_set_size(wifi_keyboard, SCREEN_WIDTH, SETTINGS_WIFI_KEYBOARD_HEIGHT);
    lv_obj_align(wifi_keyboard, LV_ALIGN_BOTTOM_MID, 0, -SETTINGS_BOTTOM_NAV_HEIGHT);
    lv_keyboard_set_textarea(wifi_keyboard, wifi_password_ta);
    lv_obj_add_flag(wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
#endif

    lv_obj_t *bottom_nav = lv_obj_create(settings_screen);
    lv_obj_set_size(bottom_nav, SCREEN_WIDTH, SETTINGS_BOTTOM_NAV_HEIGHT);
    lv_obj_align(bottom_nav, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(bottom_nav, COLOR_NAV_BG, 0);
    lv_obj_set_style_bg_opa(bottom_nav, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bottom_nav, 0, 0);
    lv_obj_set_style_radius(bottom_nav, 0, 0);
    lv_obj_set_style_pad_all(bottom_nav, 8, 0);
    lv_obj_clear_flag(bottom_nav, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(bottom_nav, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(bottom_nav, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bottom_nav, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    create_bottom_nav_btn(bottom_nav, LV_SYMBOL_HOME, settings_home_clicked, false);
    create_bottom_nav_btn_img(bottom_nav, &cube_solid_full, settings_block_clicked, false);
    create_bottom_nav_btn(bottom_nav, LV_SYMBOL_SETTINGS, settings_diagnostics_handler, true);
    create_bottom_nav_btn(bottom_nav, LV_SYMBOL_EYE_OPEN, settings_night_clicked, false);
}

void settings_screen_destroy(void)
{
    // Clean up Easter egg overlay if showing
    if (sys_overlay) {
        lv_obj_del(sys_overlay);
        sys_overlay = NULL;
    }
    diag_counter = 0;

    if (settings_screen)
    {
        lv_obj_del(settings_screen);
        settings_screen = NULL;
        miner_ip_value_label = NULL;
        auto_fan_checkbox = NULL;
        fan_slider = NULL;
        fan_value_label = NULL;
        fan_save_btn = NULL;
        wifi_dropdown = NULL;
        wifi_password_ta = NULL;
        wifi_status_label = NULL;
#if LV_USE_KEYBOARD
        wifi_keyboard = NULL;
#endif
    }
}

lv_obj_t *settings_get_screen(void)
{
    return settings_screen;
}

int settings_get_fan_speed_percent(void)
{
    return applied_fan_speed_percent;
}

float settings_get_asic_voltage_mv(void)
{
    return applied_asic_voltage_mv;
}

void settings_update_info(const settings_info_t *info)
{
    if (info)
    {
        current_settings = *info;
        applied_fan_speed_percent = info->fan_speed_percent;
        applied_asic_voltage_mv = settings_voltage_for_mode();
        update_fan_controls();

        if (auto_fan_checkbox)
        {
            if (current_settings.auto_fan_control)
            {
                lv_obj_add_state(auto_fan_checkbox, LV_STATE_CHECKED);
            }
            else
            {
                lv_obj_clear_state(auto_fan_checkbox, LV_STATE_CHECKED);
            }
        }

    }
}

void settings_auto_fan_toggled(lv_event_t *e)
{
    lv_obj_t *checkbox = lv_event_get_target(e);
    current_settings.auto_fan_control = lv_obj_has_state(checkbox, LV_STATE_CHECKED);
    update_fan_controls();
    printf("Auto fan control: %s\n", current_settings.auto_fan_control ? "ON" : "OFF");
}

void settings_fan_slider_changed(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    current_settings.fan_speed_percent = lv_slider_get_value(slider);

    if (fan_value_label)
    {
        char fan_text[16];
        snprintf(fan_text, sizeof(fan_text), "%d%%", current_settings.fan_speed_percent);
        lv_label_set_text(fan_value_label, fan_text);
    }

    printf("Fan speed set to: %d%%\n", current_settings.fan_speed_percent);
}

void settings_fan_save_clicked(lv_event_t *e)
{
    printf("Saving fan settings - Auto: %s, Speed: %d%%\n",
           current_settings.auto_fan_control ? "ON" : "OFF",
           current_settings.fan_speed_percent);

    applied_fan_speed_percent = current_settings.fan_speed_percent;
    home_update_fan_setpoint(NULL);

    if (current_settings.auto_fan_control)
    {
        BAP_send_automatic_fan_control(true);
        printf("Sending auto fan control command\n");
    }
    else
    {
        BAP_send_fan_speed(current_settings.fan_speed_percent);
        printf("Sending manual fan speed: %d%%\n", current_settings.fan_speed_percent);
    }
}

void settings_home_clicked(lv_event_t *e)
{
    home_screen_create();
    lv_scr_load(home_get_screen());
    settings_screen_destroy();
}

void settings_block_clicked(lv_event_t *e)
{
    block_screen_create();
    lv_scr_load(block_get_screen());
    settings_screen_destroy();
}

void settings_wifi_scan_clicked(lv_event_t *e)
{
    settings_wifi_clear_networks();
    if (wifi_status_label) {
        lv_label_set_text(wifi_status_label, "Scanning...");
    }

    esp_err_t ret = bap_client_request("wifiScan");
    if (ret != ESP_OK && wifi_status_label) {
        lv_label_set_text(wifi_status_label, "Scan request failed");
    }
}

void settings_wifi_connect_clicked(lv_event_t *e)
{
    if (!wifi_dropdown || !wifi_password_ta) {
        return;
    }

    char ssid[64];
    lv_dropdown_get_selected_str(wifi_dropdown, ssid, sizeof(ssid));

    if (ssid[0] == '\0' ||
        strcmp(ssid, "Tap Scan") == 0 ||
        strcmp(ssid, "Scanning...") == 0 ||
        strcmp(ssid, "No networks found") == 0) {
        if (wifi_status_label) {
            lv_label_set_text(wifi_status_label, "Pick a network first");
        }
        return;
    }

    const char *password = lv_textarea_get_text(wifi_password_ta);
    esp_err_t ssid_ret = BAP_send_wifi_ssid(ssid);
    esp_err_t pass_ret = BAP_send_wifi_password(password);
    esp_err_t connect_ret = BAP_send_wifi_connect();

    if (wifi_status_label) {
        if (ssid_ret == ESP_OK && pass_ret == ESP_OK && connect_ret == ESP_OK) {
            lv_label_set_text(wifi_status_label, "Connecting...");
        } else {
            lv_label_set_text(wifi_status_label, "Connect request failed");
        }
    }

#if LV_USE_KEYBOARD
    if (wifi_keyboard) {
        lv_obj_add_flag(wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
#endif
}

void settings_night_clicked(lv_event_t *e)
{
    // Navigate to night mode screen
    night_screen_create();
    lv_scr_load(night_get_screen());
    settings_screen_destroy();
}
