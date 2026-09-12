#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "app_events.h"
#include "services/app_ui.h"

static const char *TAG = "APP_BUTTONS";

#define GPIO_BTN_SELECT       GPIO_NUM_5
#define GPIO_BTN_ENTER        GPIO_NUM_38

#define POLL_INTERVAL_MS      100
#define DUAL_HOLD_EXIT_MS     2000

typedef enum {
    MENU_STATE_MAIN,
    MENU_STATE_IR_LEARN,
    MENU_STATE_IR_TEST
} menu_state_t;

// Sequência exata de ações exigida pelo fluxo
static const oled_cmd_action_t COMMAND_SEQUENCE[] = {
    OLED_CMD_POWER_OFF,
    OLED_CMD_POWER_ON,
    OLED_CMD_TEMP_18,
    OLED_CMD_TEMP_19,
    OLED_CMD_TEMP_20,
    OLED_CMD_TEMP_21,
    OLED_CMD_TEMP_22,
    OLED_CMD_TEMP_23,
    OLED_CMD_TEMP_24,
    OLED_CMD_TEMP_25
};

#define TOTAL_COMMANDS (sizeof(COMMAND_SEQUENCE) / sizeof(COMMAND_SEQUENCE[0]))

static menu_state_t s_current_menu = MENU_STATE_MAIN;
static uint8_t s_selected_option = 0;       // Menu principal (0: Aprender, 1: Testar)
static uint8_t s_cmd_index = 0;             // Índice da sequência de comandos
static bool s_in_exit_prompt = false;       // Flag para o prompt "Sair / Continuar"
static uint8_t s_exit_prompt_option = 0;    // Option prompt (0: Continuar, 1: Sair)

static void update_ir_screen(void) {
    oled_screen_t screen = (s_current_menu == MENU_STATE_IR_LEARN) 
                          ? OLED_SCREEN_IR_LEARN 
                          : OLED_SCREEN_IR_TEST;

    if (s_in_exit_prompt) {
        // Exibe tela de confirmação (Sair vs Continuar)
        if (s_exit_prompt_option == 0) {
            app_ui_post_message("> CONTINUAR", " SAIR", 0);
        } else {
            app_ui_post_message("  CONTINUAR ", "> SAIR", 0);
        }
    } else {
        oled_show_screen(screen, COMMAND_SEQUENCE[s_cmd_index], 0);
    }
}

static void app_buttons_task(void *pvParameters) {
    uint32_t dual_hold_timer_ms = 0;
    bool last_select = false;
    bool last_enter = false;

    app_ui_post_main_menu(s_selected_option);

    while (1) {
        // Leitura Active-High (1 = pressionado)
        bool select_pressed = (gpio_get_level(GPIO_BTN_SELECT) == 1);
        bool enter_pressed  = (gpio_get_level(GPIO_BTN_ENTER) == 1);

        // --- REGRA DO DUAL HOLD DE 5 SEGUNDOS ---
        if (select_pressed && enter_pressed) {
            dual_hold_timer_ms += POLL_INTERVAL_MS;

            if (dual_hold_timer_ms >= DUAL_HOLD_EXIT_MS) {
                ESP_LOGI(TAG, "Dual hold 5s atingido! Efetuando dadas de saida...");
                app_ui_post_message("SAINDO DO MODO", "ENVIANDO DADOS...", 1500);

                app_event_t evt = { .type = APP_EVENT_EXIT_MENU_TRIGGER_TELEMETRY };
                if (g_app_event_queue) {
                    xQueueSend(g_app_event_queue, &evt, 0);
                }

                while (gpio_get_level(GPIO_BTN_SELECT) == 1 || gpio_get_level(GPIO_BTN_ENTER) == 1) {
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                vTaskDelete(NULL);
            }
        } else {
            dual_hold_timer_ms = 0; 

            // --- TRATAMENTO DO BOTÃO SELECT (Borda de subida) ---
            if (select_pressed && !last_select) {
                if (s_current_menu == MENU_STATE_MAIN) {
                    s_selected_option = (s_selected_option == 0) ? 1 : 0;
                    app_ui_post_main_menu(s_selected_option);
                } else if (s_in_exit_prompt) {
                    // Alterna entre Continuar (0) e Sair (1)
                    s_exit_prompt_option = (s_exit_prompt_option == 0) ? 1 : 0;
                    update_ir_screen();
                }
            }

            // --- TRATAMENTO DO BOTÃO ENTER (Borda de subida) ---
            if (enter_pressed && !last_enter) {
                if (s_current_menu == MENU_STATE_MAIN) {
                    s_current_menu = (s_selected_option == 0) ? MENU_STATE_IR_LEARN : MENU_STATE_IR_TEST;
                    s_cmd_index = 0;
                    s_in_exit_prompt = false;
                    update_ir_screen();
                } else if (s_in_exit_prompt) {
                    if (s_exit_prompt_option == 1) { // Selecionou "SAIR"
                        s_current_menu = MENU_STATE_MAIN;
                        s_in_exit_prompt = false;
                        app_ui_post_main_menu(s_selected_option);
                    } else { // Selecionou "CONTINUAR"
                        s_cmd_index = 0;
                        s_in_exit_prompt = false;
                        update_ir_screen();
                    }
                } else {
                    // Incrementa na sequência de comandos
                    if (s_cmd_index < TOTAL_COMMANDS - 1) {
                        s_cmd_index++;
                        update_ir_screen();
                    } else {
                        // Chegou no comando de 25°C -> Exibe o Prompt "Sair ou Continuar"
                        s_in_exit_prompt = true;
                        s_exit_prompt_option = 0; // Padrão: Continuar
                        update_ir_screen();
                    }
                }
            }
        }

        last_select = select_pressed;
        last_enter  = enter_pressed;

        vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
    }
}

esp_err_t app_buttons_start_menu_task(void) {
    BaseType_t ret = xTaskCreate(app_buttons_task, "app_buttons_task", 8192, NULL, 3, NULL);
    return (ret == pdPASS) ? ESP_OK : ESP_FAIL;
}