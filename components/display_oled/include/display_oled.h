/**
 * @file display_oled.h
 * @brief SSD1306/SH1106 OLED Display UI Driver over Shared I2C Bus for ESP-IDF v6.0+
 * @author Embedded Software Team
 * @date 2026
 * 
 * Provides UI rendering, header info (firmware version and battery level),
 * and menu systems in Portuguese for the explorerAirConditioner project.
 */

#ifndef DISPLAY_OLED_H
#define DISPLAY_OLED_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OLED_I2C_ADDR_DEFAULT   0x3C    /**< Default I2C slave address for SSD1306 */
#define OLED_WIDTH              128     /**< Display width in pixels */
#define OLED_HEIGHT             64      /**< Display height in pixels */

/**
 * @brief Application Screen/State Identifiers.
 */
typedef enum {
    OLED_SCREEN_BOOT = 0,       /**< Boot/Initialization Screen */
    OLED_SCREEN_MENU_MAIN,      /**< Main Menu Screen (1. Aprender / 2. Testar) */
    OLED_SCREEN_IR_LEARN,       /**< IR Learning Routine Screen */
    OLED_SCREEN_IR_TEST,        /**< IR Testing Routine Screen */
    OLED_SCREEN_WIFI_ERROR,     /**< Wi-Fi Error Screen */
    OLED_SCREEN_SLEEP_PREP      /**< Deep Sleep Preparation Screen */
} oled_screen_t;

/**
 * @brief IR Command Action Labels (Portuguese UI).
 */
typedef enum {
    OLED_CMD_POWER_OFF = 0,     /**< "DESLIGAR" */
    OLED_CMD_POWER_ON,          /**< "LIGAR" */
    OLED_CMD_TEMP_18,           /**< "18 Graus" */
    OLED_CMD_TEMP_19,           /**< "19 Graus" */
    OLED_CMD_TEMP_20,           /**< "20 Graus" */
    OLED_CMD_TEMP_21,           /**< "21 Graus" */
    OLED_CMD_TEMP_22,           /**< "22 Graus" */
    OLED_CMD_TEMP_23,           /**< "23 Graus" */
    OLED_CMD_TEMP_24,           /**< "24 Graus" */
    OLED_CMD_TEMP_25,           /**< "25 Graus" */
    OLED_CMD_MAX
} oled_cmd_action_t;

/**
 * @brief Initialize the OLED display attached to the shared I2C bus.
 * 
 * @param[in] i2c_addr Target I2C slave address (e.g. 0x3C).
 * @return 
 *      - ESP_OK: Display successfully initialized.
 *      - ESP_ERR_INVALID_STATE: Shared I2C bus not initialized.
 *      - ESP_FAIL: Communication failure during initialization sequence.
 */
esp_err_t oled_init(uint8_t i2c_addr);

/**
 * @brief Deinitialize the OLED display and remove it from the shared I2C bus.
 * 
 * @return esp_err_t 
 */
esp_err_t oled_deinit(void);

/**
 * @brief Clear display frame buffer and refresh screen.
 * 
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t oled_clear(void);

/**
 * @brief Update top header metrics (Firmware Version and Battery Percentage).
 * 
 * @param[in] battery_percentage Battery level value (0 - 100%).
 * @param[in] fw_version String representation of firmware version (e.g., "v1.0").
 */
void oled_set_header_info(uint8_t battery_percentage, const char *fw_version);

/**
 * @brief Render a target UI screen state with options and header context.
 * 
 * @param[in] screen Screen enum to render.
 * @param[in] action Current active IR action (used in Learn/Test screens).
 * @param[in] selected_index Menu selection cursor index (0 or 1).
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t oled_show_screen(oled_screen_t screen, oled_cmd_action_t action, uint8_t selected_index);

/**
 * @brief Render two custom text lines centered in the display area.
 * 
 * @param[in] line1 First line string.
 * @param[in] line2 Second line string.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t oled_show_message(const char *line1, const char *line2);

/**
 * @brief Execute a standalone visual self-test procedure for display validation.
 * 
 * @note Active when CONFIG_OLED_RUN_TESTS is set in Kconfig.
 */
void oled_run_tests(void);

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_OLED_H