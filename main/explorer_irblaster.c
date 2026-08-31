#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "cJSON.h"

#include "explorer_memory.h"
#include "config.h"
#include "display.h"
#include "wifi_handler.h"
#include "ir_handler.h"
#include "mqtt_handler.h"
#include "explorer_structs.h"
#include "explorer_protocol.h"
#include "explorer_rtc.h"

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

/**
 * Sincroniza a hora (NTP) e dispara diretamente o sinal IR do agendamento 
 * retido mais próximo do horário atual.
 */
bool check_and_run_schedules(void) {
    struct tm timeinfo;
    if (!rtc_manager_get_time(&timeinfo)) {
        ESP_LOGE(TAG, "RTC sem hora válida. Ignorando agendamentos.");
        return false;
    }

    uint8_t today_bit = (1 << (timeinfo.tm_wday + 1)); // Dom=1, Seg=2, ...
    int current_minutes = (timeinfo.tm_hour * 60) + timeinfo.tm_min;

    int best_schedule_id = -1;
    int max_passed_minutes = -1;
    schedule_t best_sched;

    // --- Varre os agendamentos da NVS ---
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        schedule_t sched;
        if (explorer_memory_load_schedule((uint8_t)i, &sched)) {
            bool is_enabled = (sched.week_days & 0x01) != 0;
            bool is_today   = (sched.week_days & today_bit) != 0;

            if (is_enabled && is_today) {
                int sched_hour = 0, sched_min = 0;
                sscanf(sched.time, "%d:%d", &sched_hour, &sched_min);
                int sched_minutes = (sched_hour * 60) + sched_min;

                // Seleciona o agendamento mais recente que já passou (ou que é o atual)
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

    // --- Executa o agendamento recuperado/atual ---
    if (best_schedule_id != -1) {
        ESP_LOGI(TAG, "Agendamento válido encontrado: ID %d [%s] - Ação: %s",
                 best_schedule_id, best_sched.time, best_sched.action);

        execute_schedule_action(&best_sched); // Dispara o IR via hardware
    } else {
        ESP_LOGI(TAG, "Nenhum agendamento pendente para o dia/horário atual.");
    }

    return true;
}

static void schedule_checker_task(void *pvParameters) {
    struct tm timeinfo;
    int last_processed_minute = -1;

    ESP_LOGI(TAG, "Task de monitoramento de agendamentos (1 min) iniciada.");

    while (1) {
        if (rtc_manager_get_time(&timeinfo)) {
            // Executa apenas quando houver a virada do minuto
            if (timeinfo.tm_min != last_processed_minute) {
                last_processed_minute = timeinfo.tm_min;

                ESP_LOGI(TAG, "Mudança de minuto (%02d:%02d). Verificando agendamentos...",
                         timeinfo.tm_hour, timeinfo.tm_min);

                // Reavalia a NVS para o minuto atual
                check_and_run_schedules();
            }
        }

        // Checa a transição do tempo a cada 1 segundo
        vTaskDelay(pdMS_TO_TICKS(1000));
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

    cJSON_AddNumberToObject(root, "cmd_id", cmd_index);
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
                if (explorer_memory_save_ir(current_idx, &captured_cmd)) {
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
                                    ir_raw_command_t ir_cmd;
                                    if (explorer_memory_load_ir(cmd_index, &ir_cmd)) {
                                        ESP_LOGI(TAG, "Disparando comando IR [%s] (Índice: %d) via hardware...", action_str, cmd_index);
                                        
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
                                        ESP_LOGE(TAG, "Comando IR '%s' não encontrado na NVS (Índice: %d)", action_str, cmd_index);
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

                                if (explorer_memory_save_wifi_credentials(ssid_item->valuestring, pass_item->valuestring)) {
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

                                    if (explorer_memory_save_schedule(&sched)) {
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
                mqtt_stop();
                rtc_manager_process_schedules_and_sleep();
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
    if (!explorer_memory_init()) {
        ESP_LOGE(TAG, "Falha ao inicializar a memória!");
        display_init();
        display_power(true);
        display_show_text("MEM ERROR");
        return;
    }

    display_init();
    display_power(true);
    buttons_init();
    ir_hardware_init();

    g_mqtt_tx_queue = xQueueCreate(TOTAL_ACTIONS + 2, sizeof(mqtt_message_t));
    if (g_mqtt_tx_queue == NULL) {
        ESP_LOGE(TAG, "Falha ao criar fila TX MQTT!");
    }

    // Se ligar com GPIO5 + GPIO34 pressionados
    if (check_learning_mode_trigger()) {
        run_ir_learning_routine();
    }

    rtc_manager_init();
    bool isHourSync = check_and_run_schedules();
    xTaskCreate(schedule_checker_task, "schedule_checker_task", 3072, NULL, 4, NULL);

    // Em app_main() dentro de main.c
    if (!wifi_is_connected()) {
        // Tenta 3x na rede do cliente e 3x na rede conectaSenFio
        wifi_connect_init(on_wifi_status_change);
    }

    if (wifi_is_connected()) {
        ESP_LOGI(TAG, "Wi-Fi OK, RSSI: %d dBm", wifi_get_rssi());
        display_show_text("WIFI OK\nCONECTANDO MQTT");

        if(isHourSync == false && rtc_manager_sync_ntp() == true)
        {
            check_and_run_schedules();
        }

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

        } else {
            // Se falhar a conexão MQTT após 3 tentativas
            ESP_LOGE(TAG, "Erro na conexão MQTT!");
            display_show_text("ERRO");
            rtc_manager_process_schedules_and_sleep();
        }
    } else {
        ESP_LOGE(TAG, "Exibindo ERRO WIFI no display...");
        display_show_text("ERRO WIFI");
        rtc_manager_process_schedules_and_sleep();
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