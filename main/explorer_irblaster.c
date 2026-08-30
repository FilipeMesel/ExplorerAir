#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "cJSON.h"

#include "config.h"
#include "mqtt_handler.h"
#include "ir_handler.h"
#include "display.h"
#include "explorer_protocol.h"
#include "explorer_structs.h"

static const char *TAG = "MAIN_APP";

// Lista de estados da sequência fixa de aprendizado
static const char *LEARNING_STEPS[] = {
    "DESLIGA",
    "LIGA",
    "18 C",
    "19 C",
    "20 C",
    "21 C",
    "22 C",
    "23 C",
    "24 C",
    "25 C"
};

#define TOTAL_LEARNING_STEPS (sizeof(LEARNING_STEPS) / sizeof(LEARNING_STEPS[0]))

typedef enum {
    PROTO_STATE_IDLE,
    PROTO_STATE_WAIT_SERVER,
    PROTO_STATE_SLEEPING
} protocol_state_t;

static protocol_state_t g_proto_state = PROTO_STATE_IDLE;
static int g_sleep_seconds = 10;
static char g_last_action[32] = "NONE";
static bool g_learning_active = false;
static bool g_force_test_trigger = false;

// Buffer para armazenar os comandos IR aprendidos na sequência
static ir_raw_command_t g_learned_commands[TOTAL_LEARNING_STEPS];
static bool g_step_has_data[TOTAL_LEARNING_STEPS] = {false};

static void get_sensor_data(int *temp, int *umid, char *rtc_buf) {
    *temp = 24;
    *umid = 60;
    strcpy(rtc_buf, "10:00");
}

static void setup_buttons(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_1_GPIO) | (1ULL << BUTTON_2_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
}

static void setup_output_gpio(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_OUTPUT_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    gpio_set_level(GPIO_OUTPUT_PIN, 1);
}

// Retorna o nível do GPIO (Ajuste a checagem se seu hardware usa Pull-Up ou Pull-Down)
static bool check_test_button_pressed(void) {
    return (gpio_get_level(BUTTON_1_GPIO) != 0);
}

static bool check_learning_button_pressed(void) {
    return (gpio_get_level(BUTTON_2_GPIO) != 0);
}

// Verifica se AMBOS os botões estão pressionados simultaneamente no boot
static bool check_both_buttons_pressed_on_boot(void) {
    return (check_test_button_pressed() && check_learning_button_pressed());
}

static void safe_display_show(const char *text) {
    display_power(true);
    display_show_text(text);
}

static void ir_send_by_label(const char *label) {
    for (int i = 0; i < TOTAL_LEARNING_STEPS; i++) {
        if (strcmp(label, LEARNING_STEPS[i]) == 0 && g_step_has_data[i]) {
            ESP_LOGI(TAG, "Transmitindo IR aprendido para '%s' (tam: %d)", label, (int)g_learned_commands[i].length);
            ir_send_command(&g_learned_commands[i]);
            return;
        }
    }

    ESP_LOGI(TAG, "Transmitindo IR genérico para '%s'", label);
    ir_raw_command_t dummy_cmd;
    dummy_cmd.length = 4;
    dummy_cmd.data[0] = 9000;
    dummy_cmd.data[1] = 4500;
    dummy_cmd.data[2] = 560;
    dummy_cmd.data[3] = 560;
    ir_send_command(&dummy_cmd);
}

// Máquina de Sequência de Aprendizado Guiado (Modo Local Offline)
static bool run_guided_learning_routine(void) {
    ESP_LOGW(TAG, "Iniciando Sequência de Aprendizado IR Guiada...");
    int current_step = 0;
    ir_raw_command_t temp_ir_cmd;

    while (current_step < TOTAL_LEARNING_STEPS) {
        char disp_buf[32];
        snprintf(disp_buf, sizeof(disp_buf), "AP: %s", LEARNING_STEPS[current_step]);
        safe_display_show(disp_buf);

        // Captura do IR via sensor
        if (ir_read_last_command(&temp_ir_cmd)) {
            if (temp_ir_cmd.length > 0) {
                g_learned_commands[current_step] = temp_ir_cmd;
                g_step_has_data[current_step] = true;
                
                snprintf(disp_buf, sizeof(disp_buf), "%s OK!", LEARNING_STEPS[current_step]);
                safe_display_show(disp_buf);
                ESP_LOGI(TAG, "Passo [%s] capturado com sucesso!", LEARNING_STEPS[current_step]);
                
                vTaskDelay(pdMS_TO_TICKS(800));
            }
        }

        // Testar emissão do IR (GPIO 34)
        if (check_test_button_pressed()) {
            while (check_test_button_pressed()) vTaskDelay(pdMS_TO_TICKS(50));

            if (g_step_has_data[current_step]) {
                ESP_LOGI(TAG, "Testando IR do passo: %s", LEARNING_STEPS[current_step]);
                safe_display_show("TESTANDO IR...");
                ir_send_command(&g_learned_commands[current_step]);
                vTaskDelay(pdMS_TO_TICKS(1000));
            } else {
                safe_display_show("SEM IR GRAVADO");
                vTaskDelay(pdMS_TO_TICKS(800));
            }
        }

        // Avançar no aprendizado (GPIO 05)
        if (check_learning_button_pressed()) {
            while (check_learning_button_pressed()) vTaskDelay(pdMS_TO_TICKS(50));

            if (g_step_has_data[current_step]) {
                if (current_step < TOTAL_LEARNING_STEPS - 1) {
                    current_step++;
                    ESP_LOGI(TAG, "Avançando para o próximo passo: %s", LEARNING_STEPS[current_step]);
                } else {
                    // Chegou no 25°C e apertou GPIO 05 -> Finaliza aprendizado
                    safe_display_show("ENVIANDO FILA...");
                    ESP_LOGI(TAG, "Aprendizado finalizado em 25C! Encerrando para enviar a fila MQTT.");
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    return true;
                }
            } else {
                safe_display_show("GRAVE IR ANTES!");
                vTaskDelay(pdMS_TO_TICKS(800));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }

    return false;
}

// ============================================================================
// ENVIO DE COMANDOS (JSON)
// ============================================================================

/**
 * @brief Função unificada para envio de comandos via MQTT/JSON.
 * 
 * @param cmd_id Identificador do comando (command_id_t)
 * @param params Ponteiro para parâmetros opcionais (pode ser NULL para ACKs)
 * @return true se a publicação MQTT teve sucesso, false caso contrário.
 */
static bool send_mqtt_command(command_id_t cmd_id, const send_cmd_params_t *params) {
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        ESP_LOGE(TAG, "Falha ao alocar cJSON object");
        return false;
    }

    cJSON_AddNumberToObject(root, "comando", cmd_id);

    // Monta o payload dinamicamente de acordo com o tipo de comando
    switch (cmd_id) {
        case CMD_TELEMETRY: {
            int temp, umid;
            char rtc[8];
            get_sensor_data(&temp, &umid, rtc);

            cJSON_AddNumberToObject(root, "temp", temp);
            cJSON_AddNumberToObject(root, "umid", umid);
            cJSON_AddStringToObject(root, "rtc", rtc);
            cJSON_AddStringToObject(root, "last_action", g_last_action);
            break;
        }

        case CMD_EXECUTE_IR_ACK:
        case CMD_SET_SLEEP_ACK:
        case CMD_SET_WIFI_ACK:
            cJSON_AddStringToObject(root, "status", "OK");
            break;

        case CMD_SYNC_LEARNED:
            if (params && params->action_label && params->ir_cmd) {
                cJSON_AddStringToObject(root, "action", params->action_label);
                cJSON_AddNumberToObject(root, "length", params->ir_cmd->length);

                cJSON *raw_array = cJSON_CreateArray();
                if (raw_array) {
                    for (size_t i = 0; i < params->ir_cmd->length; i++) {
                        cJSON_AddItemToArray(raw_array, cJSON_CreateNumber(params->ir_cmd->data[i]));
                    }
                    cJSON_AddItemToObject(root, "raw_data", raw_array);
                }
            }
            break;

        default:
            ESP_LOGW(TAG, "Comando %d não possui tratamento de montagem de payload", cmd_id);
            break;
    }

    // Serialização e Log
    char *out = cJSON_PrintUnformatted(root);
    if (!out) {
        cJSON_Delete(root);
        return false;
    }

    if (cmd_id == CMD_SYNC_LEARNED && params && params->action_label) {
        ESP_LOGI(TAG, "TX -> %s [%s]: %s", get_cmd_display_name(cmd_id), params->action_label, out);
    } else {
        ESP_LOGI(TAG, "TX -> %s: %s", get_cmd_display_name(cmd_id), out);
    }

    // Transmissão
    char mac_str[13] = {0};
    bool res = mqtt_publish_commands_json(out, mac_str);

    // Cleanup de memória RAM (evita Memory Leaks)
    cJSON_Delete(root);
    free(out);

    return res;
}

// ============================================================================
// PARSER DOS COMANDOS
// ============================================================================
void protocol_parse_payload(const char *payload) {
    if (g_learning_active) return;

    cJSON *root = cJSON_Parse(payload);
    if (!root) return;

    cJSON *cmd_item = cJSON_GetObjectItem(root, "comando");
    if (cJSON_IsNumber(cmd_item)) {
        int cmd = cmd_item->valueint;

        switch (cmd) {
            case CMD_TELEMETRY_ACK:
                ESP_LOGI(TAG, "RX <- %s", get_cmd_display_name(CMD_TELEMETRY_ACK));
                safe_display_show(get_cmd_display_name(CMD_TELEMETRY_ACK));
                break;

            case CMD_EXECUTE_IR:
                {
                    cJSON *act_item = cJSON_GetObjectItem(root, "action");
                    if (cJSON_IsString(act_item)) {
                        strncpy(g_last_action, act_item->valuestring, sizeof(g_last_action) - 1);
                        ESP_LOGI(TAG, "RX <- %s: Ação '%s'", get_cmd_display_name(CMD_EXECUTE_IR), g_last_action);
                        
                        safe_display_show(g_last_action);
                        ir_send_by_label(g_last_action);

                        send_mqtt_command(CMD_EXECUTE_IR_ACK, NULL);
                    }
                }
                break;

                case CMD_SET_WIFI:
                {
                    cJSON *ssid_item = cJSON_GetObjectItem(root, "ssid");
                    cJSON *pass_item = cJSON_GetObjectItem(root, "password");

                    if (cJSON_IsString(ssid_item) && cJSON_IsString(pass_item)) {
                        ESP_LOGI(TAG, "RX <- CMD 7: Gravando nova rede Wi-Fi na NVS: %s", ssid_item->valuestring);
                        
                        wifi_save_nvs_credentials(ssid_item->valuestring, pass_item->valuestring);
                        
                        // Confirma o recebimento enviando o CMD 8 (sem reiniciar ou alterar conexão atual)
                        send_mqtt_command(CMD_SET_WIFI_ACK, NULL);
                        safe_display_show("WIFI SALVO");
                    }
                }
                break;

            case CMD_SET_SLEEP:
                {
                    cJSON *sleep_item = cJSON_GetObjectItem(root, "sleep");
                    if (cJSON_IsNumber(sleep_item)) {
                        g_sleep_seconds = sleep_item->valueint;
                        ESP_LOGI(TAG, "RX <- %s: Sleep %ds.", get_cmd_display_name(CMD_SET_SLEEP), g_sleep_seconds);
                        
                        send_mqtt_command(CMD_SET_SLEEP_ACK, NULL);
                        safe_display_show("SLEEP OK");
                        g_proto_state = PROTO_STATE_SLEEPING;
                    }
                }
                break;

            default:
                ESP_LOGW(TAG, "Comando desconhecido recebido: %d", cmd);
                break;
        }
    }

    cJSON_Delete(root);
}

// ============================================================================
// TASK PRINCIPAL (Wi-Fi e MQTT)
// ============================================================================
// Em main_app.c
static void main_application_task(void *pvParameters) {
    int sleep_counter = 0;

    while (1) {
        // Ação do usuário: Apertar o botão no estado de erro força a reconexão
        if (check_test_button_pressed()) {
            ESP_LOGI(TAG, "Botão Pressionado: Reiniciando processo de rede...");
            wifi_reset_connection_state();
            g_proto_state = PROTO_STATE_IDLE;
            
            while (check_test_button_pressed()) {
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        }

        // Se o processo de Wi-Fi falhar permanentemente, trava e exibe ERRO WIFI
        if (wifi_is_failed()) {
            safe_display_show("ERRO WIFI");
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        // Máquina de Estados Operacional
        switch (g_proto_state) {
            case PROTO_STATE_IDLE:
                safe_display_show("CONECTANDO...");
                
                // 1º ESTABELECE CONEXÃO DE REDE (NVS ou Fallback)
                wifi_connect_init();

                // 2º SÓ DISPARA SE O WI-FI E O IP ESTIVEREM EFETIVAMENTE ATIVOS
                if (wifi_is_failed()) {
                    break; // Deixa cair no tratamento de erro padrão da próxima passada
                }

                if (send_mqtt_command(CMD_TELEMETRY, NULL)) {
                    safe_display_show(get_cmd_display_name(CMD_TELEMETRY));
                    g_proto_state = PROTO_STATE_WAIT_SERVER;
                } else {
                    ESP_LOGE(TAG, "Falha na Telemetria/MQTT. Tentando novamente...");
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
                break;

            case PROTO_STATE_WAIT_SERVER:
                if (g_force_test_trigger) {
                    g_force_test_trigger = false;
                    g_proto_state = PROTO_STATE_IDLE;
                }
                break;

            case PROTO_STATE_SLEEPING:
                safe_display_show("AGUARDANDO...");
                if (sleep_counter < (g_sleep_seconds * 10) && !g_force_test_trigger) {
                    sleep_counter++;
                } else {
                    sleep_counter = 0;
                    g_force_test_trigger = false;
                    g_proto_state = PROTO_STATE_IDLE;
                }
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ============================================================================
// APP MAIN - PONTO DE ENTRADA DO FIRMWARE
// ============================================================================
void app_main(void) {
    setup_output_gpio();
    ESP_LOGI(TAG, "Inicializando Explorer IR Blaster...");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    setup_buttons();
    display_init();
    ir_hardware_init();

    // ------------------------------------------------------------------------
    // CHECAGEM DE BOOT: Só entra no Modo Aprendizado se AMBOS estiverem pressionados
    // ------------------------------------------------------------------------
    if (check_both_buttons_pressed_on_boot()) {
        ESP_LOGW(TAG, "Boot com AMBOS os botões pressionados! Iniciando Modo Aprendizado...");
        
        safe_display_show("MODO APREND");
        g_learning_active = true;

        // Aguarda o usuário soltar ambos os botões para iniciar a rotina limpa
        while (check_test_button_pressed() || check_learning_button_pressed()) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }

        // Executa a rotina local completa (totalmente offline)
        bool finished_ok = run_guided_learning_routine();
        
        g_learning_active = false;

        // Ao terminar a rotina com sucesso, conecta e envia a fila inteira
        if (finished_ok) {
            safe_display_show("CONECTANDO...");
            
            if (send_mqtt_command(CMD_TELEMETRY, NULL)) {
                safe_display_show(get_cmd_display_name(CMD_SYNC_LEARNED));
                
                for (int i = 0; i < TOTAL_LEARNING_STEPS; i++) {
                    if (g_step_has_data[i]) {
                        send_cmd_params_t params = {
                            .action_label = LEARNING_STEPS[i],
                            .ir_cmd = &g_learned_commands[i]
                        };
                        send_mqtt_command(CMD_SYNC_LEARNED, &params);
                        vTaskDelay(pdMS_TO_TICKS(200));
                    }
                }
                
                safe_display_show("FILA ENVIADA!");
                vTaskDelay(pdMS_TO_TICKS(1500));
            }
        }
    } else {
        ESP_LOGI(TAG, "Boot Normal: Iniciando pilha de rede diretamente.");
    }

    // Cria a Task principal para manter o loop Wi-Fi/MQTT rodando
    xTaskCreatePinnedToCore(
        main_application_task,
        "main_application_task",
        8192,
        NULL,
        5,
        NULL,
        1
    );
}