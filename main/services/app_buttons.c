#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "app_events.h"
#include "app_structs.h"
#include "app_storage.h"
#include "app_ir.h"
#include "ir_remote.h"
#include "services/app_ui.h"

static const char *TAG = "APP_BUTTONS";

#define GPIO_BTN_SELECT       GPIO_NUM_5
#define GPIO_BTN_ENTER        GPIO_NUM_38

#define POLL_INTERVAL_MS      100
#define DUAL_HOLD_EXIT_MS     2000   // 2 segundos de retenção para envio de dados
// #define INACTIVITY_TIMEOUT_MS (25 * 60 * 1000) // 25 Minutos de timeout

typedef enum {
    MENU_STATE_MAIN,
    MENU_STATE_IR_LEARN,
    MENU_STATE_IR_TEST
} menu_state_t;

// Sequência exata de comandos/telas IR
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

// Mapeamento direto entre o índice da tela (0 a 9) e o enum last_action_t
static const last_action_t ACTION_MAPPING[] = {
    ACTION_POWER_OFF,
    ACTION_POWER_ON,
    ACTION_SET_TEMP_18,
    ACTION_SET_TEMP_19,
    ACTION_SET_TEMP_20,
    ACTION_SET_TEMP_21,
    ACTION_SET_TEMP_22,
    ACTION_SET_TEMP_23,
    ACTION_SET_TEMP_24,
    ACTION_SET_TEMP_25
};

#define TOTAL_COMMANDS (sizeof(COMMAND_SEQUENCE) / sizeof(COMMAND_SEQUENCE[0]))

static menu_state_t s_current_menu = MENU_STATE_MAIN;
static uint8_t s_selected_option = 0;       // Menu principal (0: Aprender, 1: Testar)
static uint8_t s_cmd_index = 0;             // Índice do comando IR na sequência
static bool s_in_exit_prompt = false;       // Tela de prompt de confirmação ("SAIR" / "CONTINUAR")
static uint8_t s_exit_prompt_option = 0;    // Opção do prompt (0: Continuar, 1: Sair)

// Estado interno do Aprendizado IR
static bool s_ir_captured = false;
static ir_raw_command_t s_captured_cmd;

static void update_ir_screen(void) {
    oled_screen_t screen = (s_current_menu == MENU_STATE_IR_LEARN) 
                          ? OLED_SCREEN_IR_LEARN 
                          : OLED_SCREEN_IR_TEST;

    if (s_in_exit_prompt) {
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
    // uint32_t inactivity_timer_ms = 0;
    bool last_select = false;
    bool last_enter = false;

    app_ui_post_main_menu(s_selected_option);

    while (1) {
        // Leitura lógica ativa ALTA (1 = Pressionado)
        bool select_pressed = (gpio_get_level(GPIO_BTN_SELECT) == 1);
        bool enter_pressed  = (gpio_get_level(GPIO_BTN_ENTER) == 1);

        // --- REINICIA TIMER DE INATIVIDADE AO PRESSIONAR BOTÕES ---
        // if (select_pressed || enter_pressed) {
        //     inactivity_timer_ms = 0;
        // } else {
        //     inactivity_timer_ms += POLL_INTERVAL_MS;
        //     // 3.2.2 Timer de inatividade (25 min) para auto-shutdown da bateria
        //     if (inactivity_timer_ms >= INACTIVITY_TIMEOUT_MS) {
        //         ESP_LOGW(TAG, "Inatividade atingida (25 min). Desligando dispositivo...");
        //         app_event_t evt = { .type = APP_EVENT_SHUTDOWN_REQUESTED };
        //         if (g_app_event_queue) {
        //             xQueueSend(g_app_event_queue, &evt, 0);
        //         }
        //         vTaskDelete(NULL);
        //     }
        // }

        // --- REGRA 5-SEGUNDOS DUAL HOLD (SAÍDA FORÇADA) ---
        if (select_pressed && enter_pressed) {
            dual_hold_timer_ms += POLL_INTERVAL_MS;

            if (dual_hold_timer_ms >= DUAL_HOLD_EXIT_MS) {
                ESP_LOGI(TAG, "Dual hold 5s atingido! Efetuando dados de saída...");
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

            // =================================================================
            // 3.1.1 POLLING NÃO-BLOQUEANTE IR (MODO APRENDER)
            // =================================================================
            if (s_current_menu == MENU_STATE_IR_LEARN && !s_in_exit_prompt && !s_ir_captured) {
                ir_raw_command_t temp_cmd;
                if (ir_remote_read_last_command(&temp_cmd) == ESP_OK) {
                    if (temp_cmd.length > 0) {
                        s_captured_cmd = temp_cmd;
                        s_ir_captured = true;
                        ESP_LOGI(TAG, "Comando IR capturado no Slot %d (%d pulsos). Aguardando confirmacao.", 
                                 s_cmd_index, s_captured_cmd.length);
                        
                        // Notifica o usuário no display para Gravar (BT1) ou Descartar (BT2)
                        app_ui_post_message(" SINAL CAPTURADO", "BT1:GRAVAR BT2:DESC", 0);
                    }
                }
            }

            // =================================================================
            // MUTAÇÃO / TRATAMENTO DO BOTÃO SELECT (GPIO 5)
            // =================================================================
            if (select_pressed && !last_select) {
                if (s_current_menu == MENU_STATE_MAIN) {
                    s_selected_option = (s_selected_option == 0) ? 1 : 0;
                    app_ui_post_main_menu(s_selected_option);

                } else if (s_in_exit_prompt) {
                    s_exit_prompt_option = (s_exit_prompt_option == 0) ? 1 : 0;
                    update_ir_screen();

                } else if (s_current_menu == MENU_STATE_IR_LEARN) {
                    // 3.1.2 GPIO 5 (SELECT) para GRAVAR comando capturado
                    if (s_ir_captured) {
                        app_storage_save_ir_command(s_cmd_index, &s_captured_cmd);
                        ESP_LOGI(TAG, "Comando IR do Slot %d salvo na FRAM!", s_cmd_index);
                        
                        s_ir_captured = false;

                        // 3.1.3 Lógica de finalização do aprendizado (Ação 9 / 25°C)
                        if (s_cmd_index >= TOTAL_COMMANDS - 1) {
                            // Salva no contexto de wakeup/telemetria: last_action = 1 (Learned ACK)
                            wakeup_context_t ctx = {
                                .reason = WAKEUP_REASON_TELEMETRY,
                                .pending_action = LAST_ACTION_LEARNED_ACK
                            };
                            app_storage_save_wakeup_context(&ctx);

                            s_in_exit_prompt = true;
                            s_exit_prompt_option = 0;
                            update_ir_screen();
                        } else {
                            s_cmd_index++;
                            update_ir_screen();
                        }
                    }

                } else if (s_current_menu == MENU_STATE_IR_TEST) {
                    // 3.2.1 Navegação de ações (GPIO 5)
                    s_cmd_index = (s_cmd_index + 1) % TOTAL_COMMANDS;
                    update_ir_screen();
                }
            }

            // =================================================================
            // MUTAÇÃO / TRATAMENTO DO BOTÃO ENTER (GPIO 38)
            // =================================================================
            if (enter_pressed && !last_enter) {
                if (s_current_menu == MENU_STATE_MAIN) {
                    s_current_menu = (s_selected_option == 0) ? MENU_STATE_IR_LEARN : MENU_STATE_IR_TEST;
                    s_cmd_index = 0;
                    s_in_exit_prompt = false;
                    s_ir_captured = false;
                    update_ir_screen();

                } else if (s_in_exit_prompt) {
                    if (s_exit_prompt_option == 1) { // Selecionou "SAIR"
                        s_current_menu = MENU_STATE_MAIN;
                        s_in_exit_prompt = false;
                        app_ui_post_main_menu(s_selected_option);
                    } else { // Selecionou "CONTINUAR"
                        s_cmd_index = 0;
                        s_in_exit_prompt = false;
                        s_ir_captured = false;
                        update_ir_screen();
                    }

                } else if (s_current_menu == MENU_STATE_IR_LEARN) {
                    // 3.1.2 GPIO 38 (ENTER) para DESCARTAR e re-aguardar
                    if (s_ir_captured) {
                        ESP_LOGI(TAG, "Sinal IR descartado pelo usuario. Aguardando novo sinal...");
                        s_ir_captured = false;
                        update_ir_screen();
                    }

                } else if (s_current_menu == MENU_STATE_IR_TEST) {
                    // 3.2.1 Disparo manual de teste via RMT (GPIO 38)
                    last_action_t current_action = ACTION_MAPPING[s_cmd_index];
                    ESP_LOGI(TAG, "Modo Teste: Disparando acao %d (Slot %d)...", current_action, s_cmd_index);
                    
                    esp_err_t ret = app_ir_dispatch_action(current_action);
                    if (ret == ESP_OK) {
                        app_ui_post_message("  COMANDO IR  ", " ENVIADO SUCC! ", 1000);
                    } else {
                        app_ui_post_message("  FALHA ENVIO ", " SLOT VAZIO/ERR", 1000);
                    }
                    update_ir_screen();
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