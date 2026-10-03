#ifndef APP_UI_H
#define APP_UI_H

#include "esp_err.h"
#include "display_oled.h"
#include <stdint.h>
#include "app_structs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DISPLAY_FW_VERSION_LENGTH   8
#define DISPLAY_LINE_LENGTH         20

/**
 * @brief Enums para as ações de comandos IR exibidas na UI
 */
typedef enum {
#define X_ENUM(enum_name, label) UI_##enum_name,
    IR_COMMAND_LIST(X_ENUM)
#undef X_ENUM
    UI_IR_CMD_MAX
} ui_ir_cmd_action_t;

/**
 * @brief Tipos de comandos aceites pela fila do serviço de UI
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
 * @brief Estrutura de mensagens enviadas para a UI
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
            ui_ir_cmd_action_t action;
        } ir_step;

        struct {
            char line1[DISPLAY_LINE_LENGTH];
            char line2[DISPLAY_LINE_LENGTH];
            uint32_t display_ms;
        } message;
    } data;
} ui_msg_t;

/**
 * @brief Inicialização e encerramento do serviço de UI
 */
esp_err_t app_ui_init(void);
esp_err_t app_ui_deinit(void);

/* =========================================================================
 * APIs Thread-safe para comunicação com a UI
 * ========================================================================= */

esp_err_t app_ui_post_header(uint16_t battery_mv, const char *fw_version);
esp_err_t app_ui_post_booting(void);
esp_err_t app_ui_post_main_menu(uint8_t selected_index);
esp_err_t app_ui_post_ir_learn(ui_ir_cmd_action_t action);
esp_err_t app_ui_post_ir_test(ui_ir_cmd_action_t action);
esp_err_t app_ui_post_wifi_error(void);
esp_err_t app_ui_post_message(const char *line1, const char *line2, uint32_t display_ms);
esp_err_t app_ui_post_sleep_prep(void);
esp_err_t app_ui_post_clear(void);

#ifdef __cplusplus
}
#endif

#endif // APP_UI_H