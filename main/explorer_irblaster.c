#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include "config.h"
#include "display.h"
#include "wifi_handler.h"
#include "ir_handler.h"
#include "mqtt_handler.h"
#include "explorer_structs.h"
#include "explorer_protocol.h"

static const char *TAG = "EXPLORER_MAIN";

// Lista exata de comandos a serem aprendidos em sequência
static const char *IR_ACTIONS[] = {
    "LIGAR",
    "DESLIGAR",
    "18 C",
    "19 C",
    "20 C",
    "21 C",
    "22 C",
    "23 C",
    "24 C",
    "25 C"
};
#define TOTAL_ACTIONS (sizeof(IR_ACTIONS) / sizeof(IR_ACTIONS[0]))

// Fila para mensagens MQTT recebidas
QueueHandle_t g_mqtt_queue = NULL;      // Mensagens recebidas (RX)
QueueHandle_t g_mqtt_tx_queue = NULL;   // Mensagens para Enviar (TX)

// Configura os GPIOs dos botões
static void buttons_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_1_GPIO) | (1ULL << BUTTON_2_GPIO), // GPIO5 e GPIO34
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
}

// Checa se ambos os botões foram pressionados no boot
static bool check_learning_mode_trigger(void) {
    return (gpio_get_level(BUTTON_1_GPIO) != 0) && (gpio_get_level(BUTTON_2_GPIO) != 0);
}

// Salva o buffer raw do IR na NVS
static bool save_ir_to_nvs(const char *key, const ir_raw_command_t *cmd) {
    nvs_handle_t nvs_h;
    esp_err_t err = nvs_open(NVS_IR_NAMESPACE, NVS_READWRITE, &nvs_h);
    if (err != ESP_OK) return false;

    err = nvs_set_blob(nvs_h, key, cmd, sizeof(ir_raw_command_t));
    if (err == ESP_OK) {
        err = nvs_commit(nvs_h);
    }
    nvs_close(nvs_h);
    return (err == ESP_OK);
}

/**
 * Busca um comando IR armazenado na NVS pela chave
 */
static bool load_ir_from_nvs(const char *key, ir_raw_command_t *cmd_out) {
    nvs_handle_t nvs_h;
    esp_err_t err = nvs_open(NVS_IR_NAMESPACE, NVS_READONLY, &nvs_h);
    if (err != ESP_OK) return false;

    size_t required_size = sizeof(ir_raw_command_t);
    err = nvs_get_blob(nvs_h, key, cmd_out, &required_size);
    nvs_close(nvs_h);

    return (err == ESP_OK);
}

/**
 * Grava as novas credenciais de Wi-Fi na NVS
 */
static bool save_wifi_credentials_to_nvs(const char *ssid, const char *password) {
    nvs_handle_t nvs_h;
    esp_err_t err = nvs_open(NVS_WIFI_NAMESPACE, NVS_READWRITE, &nvs_h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao abrir NVS para Wi-Fi: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_set_str(nvs_h, "ssid", ssid);
    if (err == ESP_OK) {
        err = nvs_set_str(nvs_h, "password", password);
    }

    if (err == ESP_OK) {
        err = nvs_commit(nvs_h);
        ESP_LOGI(TAG, "Novas credenciais Wi-Fi salvas com sucesso!");
    } else {
        ESP_LOGE(TAG, "Erro ao salvar credenciais na NVS: %s", esp_err_to_name(err));
    }

    nvs_close(nvs_h);
    return (err == ESP_OK);
}

/**
 * Busca um agendamento armazenado na NVS pelo schedule_id (0 a 9)
 */
static bool load_schedule_from_nvs(uint8_t schedule_id, schedule_t *sched_out) {
    if (schedule_id >= MAX_SCHEDULES || sched_out == NULL) return false;

    nvs_handle_t nvs_h;
    esp_err_t err = nvs_open(NVS_SCHEDULE_NAMESPACE, NVS_READONLY, &nvs_h);
    if (err != ESP_OK) return false;

    char key[16];
    snprintf(key, sizeof(key), "sched_%d", schedule_id);

    size_t required_size = sizeof(schedule_t);
    err = nvs_get_blob(nvs_h, key, sched_out, &required_size);
    nvs_close(nvs_h);

    return (err == ESP_OK);
}

/**
 * Salva um agendamento na NVS baseado no seu schedule_id (0 a 9)
 */
static bool save_schedule_to_nvs(const schedule_t *sched) {
    if (sched->schedule_id >= MAX_SCHEDULES) {
        ESP_LOGE(TAG, "ID de agendamento inválido: %d (Máx: %d)", sched->schedule_id, MAX_SCHEDULES - 1);
        return false;
    }

    nvs_handle_t nvs_h;
    esp_err_t err = nvs_open(NVS_SCHEDULE_NAMESPACE, NVS_READWRITE, &nvs_h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao abrir NVS para agendamentos: %s", esp_err_to_name(err));
        return false;
    }

    char key[16];
    snprintf(key, sizeof(key), "sched_%d", sched->schedule_id);

    err = nvs_set_blob(nvs_h, key, sched, sizeof(schedule_t));
    if (err == ESP_OK) {
        err = nvs_commit(nvs_h);
        ESP_LOGI(TAG, "Agendamento [%s] ID %d salvo na NVS com sucesso! Days: 0x%02X, Time: %s, Action: %s",
                 key, sched->schedule_id, sched->week_days, sched->time, sched->action);
    } else {
        ESP_LOGE(TAG, "Erro ao salvar agendamento %s na NVS: %s", key, esp_err_to_name(err));
    }

    nvs_close(nvs_h);
    return (err == ESP_OK);
}

/**
 * Sincroniza a hora (NTP) e dispara diretamente o sinal IR do agendamento 
 * retido mais próximo do horário atual.
 */
#include "esp_sntp.h"
#include <time.h>
// Flag para controle de sincronização via callback
static volatile bool g_time_synced = false;

static void time_sync_notification_cb(struct timeval *tv) {
    ESP_LOGI(TAG, "Notificação NTP: Hora sincronizada com sucesso!");
    g_time_synced = true;
}

/**
 * Sincroniza a hora (NTP) e dispara diretamente o sinal IR do agendamento 
 * retido mais próximo do horário atual.
 */
void check_and_run_schedules(void) {
    // --- 1. SINCRONIZAÇÃO NTP ---
    ESP_LOGI(TAG, "Sincronizando hora via NTP...");
    display_show_text("SINCRONIZANDO\nHORA (NTP)...");

    g_time_synced = false;

    if (esp_sntp_enabled()) {
        esp_sntp_stop();
    }

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    
    // Registra callback de notificação de sincronização
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);

    // Servidores NTP públicos e locais
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "a.st1.ntp.br");
    esp_sntp_setservername(2, "time.google.com");
    
    esp_sntp_init();

    // Configura o Fuso Horário Brasil (UTC-3)
    setenv("TZ", "BRT3", 1);
    tzset();

    int retry = 0;
    const int retry_count = 15;

    // Loop aguarda a callback setar a flag ou estouro de timeout
    while (!g_time_synced && ++retry <= retry_count) {
        ESP_LOGI(TAG, "Aguardando sincronização NTP... (%d/%d)", retry, retry_count);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (!g_time_synced) {
        ESP_LOGE(TAG, "Falha ao sincronizar NTP. Agendamentos ignorados.");
        display_show_text("ERRO SINC NTP");
        return;
    }

    // --- 2. AVALIAÇÃO DOS AGENDAMENTOS ---
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    ESP_LOGI(TAG, "Hora NTP obtida com sucesso: %02d:%02d:%02d (Dia da semana: %d)",
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, timeinfo.tm_wday);

    // Bit 0 = Enable. Bits 1 a 7 = Dom, Seg, Ter, Qua, Qui, Sex, Sáb
    // tm_wday varia de 0 (Dom) a 6 (Sáb). Então Domingo usa o deslocamento 1.
    uint8_t today_bit = (1 << (timeinfo.tm_wday + 1)); 
    int current_minutes = (timeinfo.tm_hour * 60) + timeinfo.tm_min;

    int best_schedule_id = -1;
    int max_passed_minutes = -1;
    schedule_t best_sched;

    for (int i = 0; i < MAX_SCHEDULES; i++) {
        schedule_t sched;
        if (load_schedule_from_nvs((uint8_t)i, &sched)) {

            // Caso o Bit 0 não seja usado no backend para Enable, valide apenas o dia da semana:
            bool is_enabled = (sched.week_days & 0x01) != 0; // Exige Bit 0 alto
            bool is_today   = (sched.week_days & today_bit) != 0;

            ESP_LOGI(TAG, "Sched %d -> Enable: %d | IsToday: %d (Mask: 0x%02X, TodayBit: 0x%02X)",
                     i, is_enabled, is_today, sched.week_days, today_bit);

            if (is_enabled && is_today) {
                int sched_hour = 0, sched_min = 0;
                sscanf(sched.time, "%d:%d", &sched_hour, &sched_min);
                int sched_minutes = (sched_hour * 60) + sched_min;

                if (sched_minutes <= current_minutes) {
                    if (sched_minutes > max_passed_minutes) {
                        max_passed_minutes = sched_minutes;
                        best_schedule_id = i;
                        best_sched = sched;
                    }
                }
            }
        }
    }

    // --- 3. EXECUÇÃO DIRETA DO COMANDO IR VIA HARDWARE ---
    if (best_schedule_id != -1) {
        ESP_LOGI(TAG, "Agendamento selecionado: ID %d [%s] - Ação: %s",
                 best_schedule_id, best_sched.time, best_sched.action);

        int cmd_index = -1;
        if (strcmp(best_sched.action, "LIGAR") == 0) {
            cmd_index = 0;
        } else if (strcmp(best_sched.action, "DESLIGAR") == 0) {
            cmd_index = 1;
        } else if (strncmp(best_sched.action, "SET_TEMP_", 9) == 0) {
            int temp = atoi(&best_sched.action[9]);
            if (temp >= 18 && temp <= 25) {
                cmd_index = 2 + (temp - 18);
            }
        }

        if (cmd_index >= 0) {
            char nvs_key[15];
            snprintf(nvs_key, sizeof(nvs_key), "cmd_%d", cmd_index);

            ir_raw_command_t ir_cmd;
            if (load_ir_from_nvs(nvs_key, &ir_cmd)) {
                ESP_LOGI(TAG, "Disparando sinal IR do agendamento para [%s]...", best_sched.action);
                
                ir_send_command(&ir_cmd);

                char disp_msg[64];
                snprintf(disp_msg, sizeof(disp_msg), "AGEND EXECUTADO:\n%s", best_sched.action);
                display_show_text(disp_msg);
            } else {
                ESP_LOGE(TAG, "Comando IR do agendamento não encontrado na NVS (Chave: %s)", nvs_key);
                display_show_text("AGEND: IR NAO\nENCONTRADO");
            }
        } else {
            ESP_LOGE(TAG, "Ação do agendamento inválida: %s", best_sched.action);
        }
    } else {
        ESP_LOGI(TAG, "Nenhum agendamento pendente para o horário atual.");
    }
}

/**
 * Converte um comando IR capturado para o JSON no formato esperado
 * e enfileira na g_mqtt_tx_queue.
 */
static void enqueue_learned_ir_json(int cmd_index, const char *action_name, const ir_raw_command_t *cmd) {
    if (g_mqtt_tx_queue == NULL) return;

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return;

    cJSON_AddNumberToObject(root, "comando", cmd_index);
    cJSON_AddStringToObject(root, "action", action_name);
    cJSON_AddNumberToObject(root, "length", cmd->length);

    cJSON *raw_array = cJSON_CreateArray();
    if (raw_array != NULL) {
        for (int i = 0; i < cmd->length; i++) {
            // Acessa o array 'data' pertencente à struct ir_raw_command_t
            cJSON_AddItemToArray(raw_array, cJSON_CreateNumber(cmd->data[i]));
        }
        cJSON_AddItemToObject(root, "raw_data", raw_array);
    }

    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str != NULL) {
        mqtt_message_t tx_msg;
        snprintf(tx_msg.payload, sizeof(tx_msg.payload), "%s", json_str);

        if (xQueueSend(g_mqtt_tx_queue, &tx_msg, pdMS_TO_TICKS(100)) == pdTRUE) {
            ESP_LOGI(TAG, "Comando [%s] enfileirado para envio MQTT", action_name);
        } else {
            ESP_LOGE(TAG, "Fila TX de MQTT cheia! Falha ao enfileirar [%s]", action_name);
        }
        cJSON_free(json_str);
    }
    cJSON_Delete(root);
}

// Rotina de aprendizado do Ar-Condicionado
static void run_ir_learning_routine(void) {
    ESP_LOGI(TAG, "Iniciando modo de aprendizado IR...");
    display_show_text("MODO APRENDIZADO\nINICIANDO...");
    vTaskDelay(pdMS_TO_TICKS(1500));

    ir_raw_command_t captured_cmd;
    bool has_cmd = false;
    char display_buf[64];
    int current_idx = 0;

    // Aguarda soltar os botões antes de começar
    while (gpio_get_level(BUTTON_1_GPIO) != 0 || gpio_get_level(BUTTON_2_GPIO) != 0) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    while (current_idx < TOTAL_ACTIONS) {
        if (!has_cmd) {
            snprintf(display_buf, sizeof(display_buf), "%s\nAGUARDANDO IR...", IR_ACTIONS[current_idx]);
            display_show_text(display_buf);
        }

        // 1. Escuta o receptor RMT IR (permite sobrescrever mandando outro sinal)
        if (ir_read_last_command(&captured_cmd)) {
            has_cmd = true;
            ESP_LOGI(TAG, "Comando capturado para [%s] (Símbolos: %d)", IR_ACTIONS[current_idx], captured_cmd.length);
            
            snprintf(display_buf, sizeof(display_buf), "%s\nIR RECEBIDO!", IR_ACTIONS[current_idx]);
            display_show_text(display_buf);
        }

        // 2. Botão 1 (GPIO5): Testar quantas vezes quiser
        if (gpio_get_level(BUTTON_1_GPIO) != 0) {
            if (has_cmd) {
                snprintf(display_buf, sizeof(display_buf), "%s\nTESTANDO...", IR_ACTIONS[current_idx]);
                display_show_text(display_buf);
                
                // Emite o comando capturado via LED TX
                ir_send_command(&captured_cmd);
                vTaskDelay(pdMS_TO_TICKS(400));

                snprintf(display_buf, sizeof(display_buf), "%s\nIR RECEBIDO!", IR_ACTIONS[current_idx]);
                display_show_text(display_buf);
            } else {
                snprintf(display_buf, sizeof(display_buf), "%s\nNENHUM IR LIDO", IR_ACTIONS[current_idx]);
                display_show_text(display_buf);
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            
            while (gpio_get_level(BUTTON_1_GPIO) != 0) vTaskDelay(pdMS_TO_TICKS(50));
        }

        // 3. Botão 2 (GPIO34): Gravar na NVS e passar para o próximo
        if (gpio_get_level(BUTTON_2_GPIO) != 0) {
            if (has_cmd) {
                char nvs_key[15];
                snprintf(nvs_key, sizeof(nvs_key), "cmd_%d", current_idx);

                if (save_ir_to_nvs(nvs_key, &captured_cmd)) {
                    ESP_LOGI(TAG, "Comando [%s] gravado na NVS!", IR_ACTIONS[current_idx]);
                    
                    // Enfileira mensagem JSON na fila MQTT TX
                    enqueue_learned_ir_json(current_idx, IR_ACTIONS[current_idx], &captured_cmd);

                    snprintf(display_buf, sizeof(display_buf), "%s\nCOMANDO OK!", IR_ACTIONS[current_idx]);
                } else {
                    ESP_LOGE(TAG, "Erro ao salvar [%s] na NVS!", IR_ACTIONS[current_idx]);
                    snprintf(display_buf, sizeof(display_buf), "%s\nERRO AO SALVAR", IR_ACTIONS[current_idx]);
                }
                display_show_text(display_buf);
                vTaskDelay(pdMS_TO_TICKS(1200));

                current_idx++;
                has_cmd = false;
            } else {
                snprintf(display_buf, sizeof(display_buf), "%s\nENVIE O IR PRIMEIRO", IR_ACTIONS[current_idx]);
                display_show_text(display_buf);
                vTaskDelay(pdMS_TO_TICKS(1000));
            }

            while (gpio_get_level(BUTTON_2_GPIO) != 0) vTaskDelay(pdMS_TO_TICKS(50));
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }

    display_show_text("APRENDIZADO\nCONCLUIDO!");
    ESP_LOGI(TAG, "Todos os comandos de AC foram gravados com sucesso!");
    vTaskDelay(pdMS_TO_TICKS(2000));
}

// Task que consome as mensagens recebidas via MQTT
static void mqtt_consumer_task(void *pvParameters) {
    // Alocado estaticamente para NÃO consumir a pilha (stack) da task
    static mqtt_message_t msg;
    int inactivity_counter = 0;
    
    ESP_LOGI(TAG, "Task consumidora MQTT iniciada com sucesso.");

    while (1) {
        // Aguarda pacote por no máximo 1 segundo (1000ms)
        if (xQueueReceive(g_mqtt_queue, &msg, pdMS_TO_TICKS(1000)) == pdTRUE) {
            // Comando recebido: reseta o contador de inatividade
            inactivity_counter = 0;
            ESP_LOGI(TAG, "Mensagem recebida da fila: %s", msg.payload);

            cJSON *json = cJSON_Parse(msg.payload);
            if (json == NULL) {
                ESP_LOGE(TAG, "Erro ao realizar parse do JSON MQTT");
                continue;
            }

            cJSON *cmd_item = cJSON_GetObjectItem(json, "cmd_id");
            if (cJSON_IsNumber(cmd_item)) {
                command_id_t cmd_id = (command_id_t)cmd_item->valueint;
                ESP_LOGI(TAG, "Processando Comando: [%s] (ID: %d)", get_cmd_display_name(cmd_id), cmd_id);

                switch (cmd_id) {
                    case CMD_TELEMETRY:
                        ESP_LOGI(TAG, "Tratando CMD_TELEMETRY");
                        display_show_text("CMD: TELEMETRIA");
                        break;

                    case CMD_TELEMETRY_ACK:
                        ESP_LOGI(TAG, "Tratando CMD_TELEMETRY_ACK");
                        break;

                    case CMD_EXECUTE_IR:
                        {
                            ESP_LOGI(TAG, "Tratando CMD_EXECUTE_IR");
                            display_show_text("CMD: EXECUTAR IR");

                            cJSON *action_item = cJSON_GetObjectItem(json, "action");
                            if (cJSON_IsString(action_item) && action_item->valuestring != NULL) {
                                const char *action_str = action_item->valuestring;
                                int cmd_index = -1;

                                // Mapeamento da string da 'action' recebida para o índice NVS
                                if (strcmp(action_str, "LIGAR") == 0) {
                                    cmd_index = 0;
                                } else if (strcmp(action_str, "DESLIGAR") == 0) {
                                    cmd_index = 1;
                                } else if (strncmp(action_str, "SET_TEMP_", 9) == 0) {
                                    int temp = atoi(&action_str[9]); // Extrai o valor numérico após "SET_TEMP_"
                                    if (temp >= 18 && temp <= 25) {
                                        // Índices 2 a 9 mapeiam para as temperaturas 18°C a 25°C
                                        cmd_index = 2 + (temp - 18);
                                    }
                                }

                                if (cmd_index >= 0) {
                                    char nvs_key[15];
                                    snprintf(nvs_key, sizeof(nvs_key), "cmd_%d", cmd_index);

                                    ir_raw_command_t ir_cmd;
                                    if (load_ir_from_nvs(nvs_key, &ir_cmd)) {
                                        ESP_LOGI(TAG, "Enviando sinal IR para [%s] (Chave NVS: %s)...", action_str, nvs_key);
                                        
                                        // Dispara o sinal infravermelho via hardware
                                        ir_send_command(&ir_cmd);

                                        char disp_msg[64];
                                        snprintf(disp_msg, sizeof(disp_msg), "IR ENVIADO:\n%s", action_str);
                                        display_show_text(disp_msg);

                                        // Prepara e envia o ACK de execução IR (cmd_id: 4)
                                        mqtt_message_t ack_msg;
                                        snprintf(ack_msg.payload, sizeof(ack_msg.payload),
                                                "{\"cmd_id\":%d,\"status\":\"OK\",\"action\":\"%s\"}", CMD_EXECUTE_IR_ACK, action_str);

                                        if (xQueueSend(g_mqtt_tx_queue, &ack_msg, pdMS_TO_TICKS(MQTT_TX_QUEUE_SEND_TIMOUT_TICKS)) != pdTRUE) {
                                            ESP_LOGE(TAG, "Falha ao enfileirar ACK de execução IR!");
                                        }

                                    } else {
                                        ESP_LOGE(TAG, "Comando IR '%s' não encontrado na NVS (Chave: %s)", action_str, nvs_key);
                                        display_show_text("IR NAO GRAVADO");
                                    }
                                } else {
                                    ESP_LOGE(TAG, "Ação IR inválida ou não suportada: %s", action_str);
                                    display_show_text("ACAO IR INVALIDA");
                                }
                            } else {
                                ESP_LOGE(TAG, "Campo 'action' ausente ou inválido no JSON!");
                                display_show_text("ERRO JSON IR");
                            }
                        }
                        break;

                    case CMD_EXECUTE_IR_ACK:
                        ESP_LOGI(TAG, "Tratando CMD_EXECUTE_IR_ACK");
                        break;

                    case CMD_SET_SLEEP:
                        ESP_LOGI(TAG, "Tratando CMD_SET_SLEEP");
                        display_show_text("CMD: SET SLEEP");
                        break;

                    case CMD_SET_SLEEP_ACK:
                        ESP_LOGI(TAG, "Tratando CMD_SET_SLEEP_ACK");
                        break;

                    case CMD_SYNC_LEARNED:
                        ESP_LOGI(TAG, "Tratando CMD_SYNC_LEARNED");
                        display_show_text("CMD: SYNC APREND");
                        break;

                    case CMD_SET_WIFI:
                        {
                            ESP_LOGI(TAG, "Tratando CMD_SET_WIFI");
                            display_show_text("CMD: CONFIG WIFI");

                            cJSON *ssid_item = cJSON_GetObjectItem(json, "ssid");
                            cJSON *pass_item = cJSON_GetObjectItem(json, "password");

                            if (cJSON_IsString(ssid_item) && cJSON_IsString(pass_item)) {
                                ESP_LOGI(TAG, "Nova rede recebida. SSID: %s", ssid_item->valuestring);

                                display_show_text("SALVANDO WIFI...");

                                if (save_wifi_credentials_to_nvs(ssid_item->valuestring, pass_item->valuestring)) {
                                    display_show_text("WIFI ATUALIZADO");
                                    // 1. Prepara e envia a mensagem ACK para a fila TX
                                    mqtt_message_t ack_msg;
                                    snprintf(ack_msg.payload, sizeof(ack_msg.payload),
                                            "{\"cmd_id\":%d,\"status\":\"OK\"}", CMD_SET_WIFI_ACK);

                                    if (xQueueSend(g_mqtt_tx_queue, &ack_msg, pdMS_TO_TICKS(MQTT_TX_QUEUE_SEND_TIMOUT_TICKS)) != pdTRUE) {
                                        ESP_LOGE(TAG, "Falha ao enfileirar ACK de Wi-Fi!");
                                    }

                                } else {
                                    display_show_text("ERRO AO SALVAR\nWIFI");
                                }
                            } else {
                                ESP_LOGE(TAG, "Campos 'ssid' ou 'password' ausentes/inválidos no JSON!");
                                display_show_text("ERRO CREDENCIAIS");
                            }
                        }
                        break;

                        case CMD_SET_SCHEDULE:
                        {
                            ESP_LOGI(TAG, "Tratando CMD_SET_SCHEDULE");
                            display_show_text("CMD: SET AGEND");

                            cJSON *sched_id_item = cJSON_GetObjectItem(json, "schedule_id");
                            cJSON *week_days_item = cJSON_GetObjectItem(json, "week_days");
                            cJSON *time_item = cJSON_GetObjectItem(json, "time");
                            cJSON *action_item = cJSON_GetObjectItem(json, "action");

                            if (cJSON_IsNumber(sched_id_item) && cJSON_IsNumber(week_days_item) &&
                                cJSON_IsString(time_item) && cJSON_IsString(action_item)) {

                                int sched_id = sched_id_item->valueint;

                                if (sched_id >= 0 && sched_id < MAX_SCHEDULES) {
                                    schedule_t sched;
                                    memset(&sched, 0, sizeof(schedule_t));
                                    
                                    sched.schedule_id = (uint8_t)sched_id;
                                    sched.week_days = (uint8_t)week_days_item->valueint;
                                    snprintf(sched.time, sizeof(sched.time), "%s", time_item->valuestring);
                                    snprintf(sched.action, sizeof(sched.action), "%s", action_item->valuestring);

                                    if (save_schedule_to_nvs(&sched)) {
                                        char disp_msg[64];
                                        snprintf(disp_msg, sizeof(disp_msg), "AGEND %d SALVO\n%s %s", sched_id, sched.time, sched.action);
                                        display_show_text(disp_msg);

                                        // 1. Prepara o ACK do Agendamento (cmd_id: 10)
                                        mqtt_message_t ack_msg;
                                        snprintf(ack_msg.payload, sizeof(ack_msg.payload),
                                                 "{\"cmd_id\":%d,\"status\":\"OK\",\"schedule_id\":%d}",
                                                 CMD_SET_SCHEDULE_ACK, sched_id);

                                        // 2. Enfileira para envio via MQTT TX
                                        if (xQueueSend(g_mqtt_tx_queue, &ack_msg, pdMS_TO_TICKS(MQTT_TX_QUEUE_SEND_TIMOUT_TICKS)) != pdTRUE) {
                                            ESP_LOGE(TAG, "Falha ao enfileirar ACK do agendamento!");
                                        }
                                    } else {
                                        display_show_text("ERRO SALVAR\nAGENDAMENTO");
                                    }
                                } else {
                                    ESP_LOGE(TAG, "schedule_id %d fora do limite (0 a %d)", sched_id, MAX_SCHEDULES - 1);
                                    display_show_text("ID AGEND INVALIDO");
                                }
                            } else {
                                ESP_LOGE(TAG, "Campos ausentes/inválidos no JSON de Agendamento!");
                                display_show_text("ERRO JSON AGEND");
                            }
                        }
                        break;

                    case CMD_SET_SCHEDULE_ACK:
                        ESP_LOGI(TAG, "Tratando CMD_SET_SCHEDULE_ACK");
                        break;

                    default:
                        ESP_LOGW(TAG, "Comando desconhecido recebido: %d", cmd_id);
                        display_show_text("CMD DESCONHECIDO");
                        break;
                }
            } else {
                ESP_LOGE(TAG, "Campo 'cmd_id' não encontrado no JSON");
            }

            cJSON_Delete(json);
        } else {
            // Nenhum pacote recebido nos últimos 1000ms: incrementa o timer
            inactivity_counter++;
            ESP_LOGD(TAG, "Inatividade: %d/%d s", inactivity_counter, MQTT_INTERACTIVITY_TIMEOUT);

            if (inactivity_counter >= MQTT_INTERACTIVITY_TIMEOUT) {
                ESP_LOGW(TAG, "Tempo limite de inatividade (%d s) atingido. Encerrando task...", MQTT_INTERACTIVITY_TIMEOUT);
                display_clear();
                
                // Finaliza e remove a própria task da memória do FreeRTOS
                // TODO: Botar esp32 para dormir aqui!
                vTaskDelete(NULL);
            }
        }
    }
}

// Esvazia a fila de saída e envia todos os JSONs gravados via MQTT
static void flush_mqtt_tx_queue(void) {
    mqtt_message_t tx_msg;
    int count = 0;

    while (xQueueReceive(g_mqtt_tx_queue, &tx_msg, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (mqtt_publish_commands_json(tx_msg.payload, NULL)) {
            count++;
        } else {
            ESP_LOGE(TAG, "Falha ao publicar comando IR via MQTT");
        }
        vTaskDelay(pdMS_TO_TICKS(200)); // Pequeno delay entre publicações
    }
}

/**
 * Monta e enfileira a telemetria inicial com o RSSI atual
 * para envio via fila TX de MQTT.
 */
bool send_telemetry(void) {
    if (g_mqtt_tx_queue == NULL) return false;

    int current_rssi = wifi_get_rssi();
    mqtt_message_t tx_msg;

    snprintf(tx_msg.payload, sizeof(tx_msg.payload),
             "{\"cmd_id\":0,\"temp\":24,\"umid\":60,\"rtc\":\"10:00\",\"RSSI\":%d}",
             current_rssi);

    if (xQueueSend(g_mqtt_tx_queue, &tx_msg, pdMS_TO_TICKS(100)) == pdTRUE) {
        ESP_LOGI(TAG, "Telemetria enfileirada com sucesso: %s", tx_msg.payload);
        return true;
    } else {
        ESP_LOGE(TAG, "Falha ao enfileirar telemetria. Fila TX cheia!");
        return false;
    }
}

// Callback do Wi-Fi
static void on_wifi_status_change(const char *title, const char *subtitle) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%s\n%s", title, subtitle);
    display_show_text(buffer);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NEW_VERSION_FOUND || ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    display_init();
    display_power(true);
    buttons_init();
    ir_hardware_init();

    // TODO: A leitura dos agendamentos deve ficar aqui
    // check_and_run_schedules();

    g_mqtt_tx_queue = xQueueCreate(TOTAL_ACTIONS + 2, sizeof(mqtt_message_t));
    if (g_mqtt_tx_queue == NULL) {
        ESP_LOGE(TAG, "Falha ao criar fila TX MQTT!");
    }

    // Se ligar com GPIO5 + GPIO34 pressionados
    if (check_learning_mode_trigger()) {
        run_ir_learning_routine();
    }

    // Em app_main() dentro de main.c
    if (!wifi_is_connected()) {
        // Tenta 3x na rede do cliente e 3x na rede conectaSenFio
        wifi_connect_init(on_wifi_status_change);
    }

    if (wifi_is_connected()) {
        ESP_LOGI(TAG, "Wi-Fi OK, RSSI: %d dBm", wifi_get_rssi());
        display_show_text("WIFI OK\nCONECTANDO MQTT");

        // 2. Tenta conectar no MQTT 3 Vezes
        if (mqtt_init_with_retry(3)) {
            display_show_text("WIFI OK\nONLINE");

            // Publica status no tópico e sobrescreve (Retain)
            mqtt_publish_status("{\"status\":\"online\"}", false);

            // 3. Cria Fila e inicia a Task consumidora dos comandos
            g_mqtt_queue = xQueueCreate(10, sizeof(mqtt_message_t));
            if (g_mqtt_queue != NULL) {
                xTaskCreate(mqtt_consumer_task, "mqtt_consumer_task", 8192, NULL, 5, NULL);
            } else {
                ESP_LOGE(TAG, "Falha ao criar fila MQTT!");
            }

            // 4. Envia telemetria inicial via MQTT
            send_telemetry();

            //TODO: A leitura dos agendamentos deve sair aqui
            check_and_run_schedules();

        } else {
            // Se falhar a conexão MQTT após 3 tentativas
            ESP_LOGE(TAG, "Erro na conexão MQTT!");
            display_show_text("ERRO");
        }
    } else {
        ESP_LOGE(TAG, "Exibindo ERRO WIFI no display...");
        display_show_text("ERRO WIFI");
    }

    while (1) {

        if(mqtt_is_connected())
        {
            // Garante o envio imediato da mensagem pendente na fila ao broker
            flush_mqtt_tx_queue();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}