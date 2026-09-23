#ifndef APP_UI_H
#define APP_UI_H

#include "esp_err.h"
#include "display_oled.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DISPLAY_FW_VERSION_LENGTH   8
#define DISPLAY_LINE_LENGTH         20

/**
 * @brief Types of commands accepted by the UI service queue
 */
typedef enum {
    UI_CMD_UPDATE_HEADER,
    UI_CMD_SHOW_BOOT,
    UI_CMD_SHOW_MAIN_MENU,
    UI_CMD_SHOW_IR_LEARN,
    UI_CMD_SHOW_IR_TEST,
    UI_CMD_SHOW_WIFI_ERROR,
    UI_CMD_SHOW_MESSAGE,
    UI_CMD_SHOW_SLEEP_PREP,
    UI_CMD_CLEAR
} ui_cmd_type_t;

/**
 * @brief Structure of the message transmitted on the UI queue
 */
typedef struct {
    ui_cmd_type_t type;
    union {
        struct {
            uint16_t battery_mv;
            char fw_version[DISPLAY_FW_VERSION_LENGTH];
        } header;
        
        struct {
            uint8_t selected_index;
        } main_menu;

        struct {
            oled_cmd_action_t action;
        } ir_step;

        struct {
            char line1[DISPLAY_LINE_LENGTH];
            char line2[DISPLAY_LINE_LENGTH];
            uint32_t display_ms;
        } message;
    } data;
} ui_msg_t;

/**
 * @brief Initializes the display, creates the app_ui queue, and launches the UI Task.
 */
esp_err_t app_ui_init(void);

/**
 * @brief Terminates the UI service and destroys resources.
 */
esp_err_t app_ui_deinit(void);

/* =========================================================================
 * APIs Thread-safe for sending messages to the UI
 * ========================================================================= */

esp_err_t app_ui_post_header(uint16_t battery_mv, const char *fw_version);
esp_err_t app_ui_post_booting(void);
esp_err_t app_ui_post_main_menu(uint8_t selected_index);
esp_err_t app_ui_post_wifi_error(void);
esp_err_t app_ui_post_message(const char *line1, const char *line2, uint32_t display_ms);
esp_err_t app_ui_post_sleep_prep(void);
esp_err_t app_ui_post_clear(void);

#ifdef __cplusplus
}
#endif

#endif // APP_UI_H