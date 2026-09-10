/**
 * @file app_ui.h
 * @brief User Interface Management Service for explorerAirConditioner.
 * @author Embedded Software Architecture Team
 * @date 2026
 * 
 * High-level orchestration layer for the OLED display. Encapsulates state transitions,
 * battery reporting, Portuguese UI rendering, and user feedback logic.
 */

#ifndef APP_UI_H
#define APP_UI_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "display_oled.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the UI Service and underlying OLED display hardware.
 * 
 * @return 
 *      - ESP_OK: UI service initialized successfully.
 *      - ESP_ERR_INVALID_STATE: I2C bus handle invalid or display initialization failed.
 */
esp_err_t app_ui_init(void);

/**
 * @brief Deinitialize the UI Service and power down/clear the display.
 * 
 * @return ESP_OK on success.
 */
esp_err_t app_ui_deinit(void);

/**
 * @brief Update system parameters shown in the header (Battery % and FW version).
 * 
 * @param[in] battery_mv Battery voltage in millivolts (e.g. 3700 = 3.7V).
 * @param[in] fw_version Firmware version string (e.g. "v1.0").
 */
void app_ui_update_header(uint16_t battery_mv, const char *fw_version);

/**
 * @brief Render Main Menu (Aprender vs Testar).
 * 
 * @param[in] selected_index 0 for "1. APRENDER", 1 for "2. TESTAR".
 * @return ESP_OK on success.
 */
esp_err_t app_ui_show_main_menu(uint8_t selected_index);

/**
 * @brief Render IR Learning Step Screen.
 * 
 * @param[in] action Current action being learned.
 * @return ESP_OK on success.
 */
esp_err_t app_ui_show_ir_learn_step(oled_cmd_action_t action);

/**
 * @brief Render IR Test Mode Screen.
 * 
 * @param[in] action Current active action to test/transmit.
 * @return ESP_OK on success.
 */
esp_err_t app_ui_show_ir_test_step(oled_cmd_action_t action);

/**
 * @brief Render Wi-Fi Connection Error Screen.
 * 
 * Displays "ERRO WIFI" and "SEM CONEXAO!".
 * 
 * @return ESP_OK on success.
 */
esp_err_t app_ui_show_wifi_error(void);

/**
 * @brief Render a temporary generic message centered on the OLED display.
 * 
 * @param[in] line1 First line string.
 * @param[in] line2 Second line string (optional, can be NULL).
 * @param[in] display_ms Duration in milliseconds to show message (0 = keep indefinitely).
 * @return ESP_OK on success.
 */
esp_err_t app_ui_show_message(const char *line1, const char *line2, uint32_t display_ms);

/**
 * @brief Show Boot Sequence Banner.
 * 
 * @return ESP_OK on success.
 */
esp_err_t app_ui_show_booting(void);

/**
 * @brief Show Deep Sleep transition banner before power hold release.
 * 
 * @return ESP_OK on success.
 */
esp_err_t app_ui_show_sleep_prep(void);

/**
 * @brief Clear display and set into ultra-low power standby mode.
 * 
 * @return ESP_OK on success.
 */
esp_err_t app_ui_clear(void);

#ifdef __cplusplus
}
#endif

#endif // APP_UI_H