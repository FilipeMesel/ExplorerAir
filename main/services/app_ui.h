#ifndef APP_UI_H
#define APP_UI_H

#include "esp_err.h"
#include "display_oled.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Tipos de comandos aceitos pela fila do serviço de UI
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
 * @brief Estrutura da mensagem trafegada na fila da UI
 */
typedef struct {
    ui_cmd_type_t type;
    union {
        struct {
            uint16_t battery_mv;
            char fw_version[8];
        } header;
        
        struct {
            uint8_t selected_index;
        } main_menu;

        struct {
            oled_cmd_action_t action;
        } ir_step;

        struct {
            char line1[20];
            char line2[20];
            uint32_t display_ms;
        } message;
    } data;
} ui_msg_t;

/**
 * @brief Inicializa o display, cria a fila do app_ui e dispara a Task UI.
 */
esp_err_t app_ui_init(void);

/**
 * @brief Finaliza o serviço de UI e destrói recursos.
 */
esp_err_t app_ui_deinit(void);

/* =========================================================================
 * APIs Thread-Safe para envio de mensagens à UI
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