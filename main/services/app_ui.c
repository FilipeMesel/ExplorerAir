/**
 * @file app_ui.c
 * @brief High-Level UI Service Implementation.
 * @author Embedded Software Architecture Team
 * @date 2026
 */

#include "services/app_ui.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "APP_UI";

#define BATTERY_MAX_MV 4200
#define BATTERY_MIN_MV 3300

static uint8_t convert_mv_to_percentage(uint16_t battery_mv) {
    if (battery_mv >= BATTERY_MAX_MV) return 100;
    if (battery_mv <= BATTERY_MIN_MV) return 0;
    
    return (uint8_t)(((uint32_t)(battery_mv - BATTERY_MIN_MV) * 100) / (BATTERY_MAX_MV - BATTERY_MIN_MV));
}

esp_err_t app_ui_init(void) {
    ESP_LOGI(TAG, "Initializing App UI Service...");
    esp_err_t ret = oled_init(OLED_I2C_ADDR_DEFAULT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize OLED driver via UI service: %s", esp_err_to_name(ret));
        return ret;
    }
    
    oled_set_header_info(100, "v1.0");
    return ESP_OK;
}

esp_err_t app_ui_deinit(void) {
    ESP_LOGI(TAG, "Deinitializing App UI Service...");
    oled_clear();
    return oled_deinit();
}

void app_ui_update_header(uint16_t battery_mv, const char *fw_version) {
    uint8_t pct = convert_mv_to_percentage(battery_mv);
    oled_set_header_info(pct, fw_version != NULL ? fw_version : "v1.0");
}

esp_err_t app_ui_show_main_menu(uint8_t selected_index) {
    return oled_show_screen(OLED_SCREEN_MENU_MAIN, OLED_CMD_POWER_OFF, selected_index);
}

esp_err_t app_ui_show_ir_learn_step(oled_cmd_action_t action) {
    return oled_show_screen(OLED_SCREEN_IR_LEARN, action, 0);
}

esp_err_t app_ui_show_ir_test_step(oled_cmd_action_t action) {
    return oled_show_screen(OLED_SCREEN_IR_TEST, action, 0);
}

esp_err_t app_ui_show_wifi_error(void) {
    ESP_LOGW(TAG, "Displaying Wi-Fi Error Screen");
    return oled_show_screen(OLED_SCREEN_WIFI_ERROR, OLED_CMD_POWER_OFF, 0);
}

esp_err_t app_ui_show_message(const char *line1, const char *line2, uint32_t display_ms) {
    esp_err_t ret = oled_show_message(line1, line2);
    if (ret == ESP_OK && display_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(display_ms));
    }
    return ret;
}

esp_err_t app_ui_show_booting(void) {
    return oled_show_screen(OLED_SCREEN_BOOT, OLED_CMD_POWER_OFF, 0);
}

esp_err_t app_ui_show_sleep_prep(void) {
    return oled_show_screen(OLED_SCREEN_SLEEP_PREP, OLED_CMD_POWER_OFF, 0);
}

esp_err_t app_ui_clear(void) {
    return oled_clear();
}