/**
 * @file explorer_irblaster.c
 * @author Filipe Mesel Lobo Costa Cardoso
 * @brief This file contains the main implementation for the Explorer IR Blaster project.
 * @version 0.1
 * @date 2026-08-31
 * 
 * @copyright Copyright (c) 2026
 * 
 */

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

/** @brief Array of strings representing the available IR actions */
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
#define TOTAL_ACTIONS (sizeof(IR_ACTIONS) / sizeof(IR_ACTIONS[0]))  /** < Total number of available IR actions */

/** @brief Queue handle for MQTT messages received */
QueueHandle_t g_mqtt_queue = NULL;
/** @brief Queue handle for MQTT messages to be sent */
QueueHandle_t g_mqtt_tx_queue = NULL;

/** @brief Initializes the buttons GPIOs */
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

/** @brief Checks if both buttons were pressed during boot */
static bool check_learning_mode_trigger(void) {
    return (gpio_get_level(BUTTON_1_GPIO) != 0) && (gpio_get_level(BUTTON_2_GPIO) != 0);
}

/**
 * @brief Checks for pending schedules and runs the most recent one
 * 
 * @return true if a schedule was executed, false otherwise 
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

    // --- Run through all schedules to find the most recent one that has passed ---
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        schedule_t sched;
        if (explorer_memory_load_schedule((uint8_t)i, &sched)) {
            bool is_enabled = (sched.week_days & 0x01) != 0;
            bool is_today   = (sched.week_days & today_bit) != 0;

            if (is_enabled && is_today) {
                int sched_hour = 0, sched_min = 0;
                sscanf(sched.time, "%d:%d", &sched_hour, &sched_min);
                int sched_minutes = (sched_hour * 60) + sched_min;

                // Select the most recent schedule that has passed (closest to the current time)
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

    // --- Execute the found schedule ---
    if (best_schedule_id != -1) {
        ESP_LOGI(TAG, "Agendamento válido encontrado: ID %d [%s] - Ação: %s",
                 best_schedule_id, best_sched.time, best_sched.action);

        execute_schedule_action(&best_sched); // Trigger the IR command associated with the schedule
    } else {
        ESP_LOGI(TAG, "Nenhum agendamento pendente para o dia/horário atual.");
    }

    return true;
}

/**
 * @brief Task for checking pending schedules every minute and executing the most recent one if applicable.
 * 
 * @param pvParameters Pointer to task parameters (not used)
 */
static void schedule_checker_task(void *pvParameters) {
    struct tm timeinfo;
    int last_processed_minute = -1;

    ESP_LOGI(TAG, "Task de monitoramento de agendamentos (1 min) iniciada.");

    while (1) {
        if (rtc_manager_get_time(&timeinfo)) {
            // Execute the schedule only if the minute has changed since the last check
            if (timeinfo.tm_min != last_processed_minute) {
                last_processed_minute = timeinfo.tm_min;

                ESP_LOGI(TAG, "Mudança de minuto (%02d:%02d). Verificando agendamentos...",
                         timeinfo.tm_hour, timeinfo.tm_min);

                // Check and execute any pending schedules
                check_and_run_schedules();
            }
        }

        // Check the schedule every second to ensure we catch the minute change
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * @brief Convert a learned IR command into a JSON string and enqueue it for MQTT transmission.
 * 
 * @param cmd_index Ir command index (0 to TOTAL_ACTIONS - 1)
 * @param action_name Name of the action associated with the command
 * @param cmd Pointer to the captured IR command
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
            // Get the raw data and add it to the JSON array
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

/**
 * @brief Run the IR learning routine
 * 
 */
static void run_ir_learning_routine(void) {
    ESP_LOGI(TAG, "Iniciando modo de aprendizado IR...");
    display_show_text("MODO APRENDIZADO\nINICIANDO...");
    vTaskDelay(pdMS_TO_TICKS(1500));

    ir_raw_command_t captured_cmd;
    bool has_cmd = false;
    char display_buf[64];
    int current_idx = 0;

    // Wait for both buttons to be released before starting the learning routine
    while (gpio_get_level(BUTTON_1_GPIO) != 0 || gpio_get_level(BUTTON_2_GPIO) != 0) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    while (current_idx < TOTAL_ACTIONS) {
        if (!has_cmd) {
            snprintf(display_buf, sizeof(display_buf), "%s\nAGUARDANDO IR...", IR_ACTIONS[current_idx]);
            display_show_text(display_buf);
        }

        // 1. Listen for the IR receiver
        if (ir_read_last_command(&captured_cmd)) {
            has_cmd = true;
            ESP_LOGI(TAG, "Comando capturado para [%s] (Símbolos: %d)", IR_ACTIONS[current_idx], captured_cmd.length);
            
            snprintf(display_buf, sizeof(display_buf), "%s\nIR RECEBIDO!", IR_ACTIONS[current_idx]);
            display_show_text(display_buf);
        }

        // 2. Button 1 (GPIO5): Test the captured command via LED TX
        if (gpio_get_level(BUTTON_1_GPIO) != 0) {
            if (has_cmd) {
                snprintf(display_buf, sizeof(display_buf), "%s\nTESTANDO...", IR_ACTIONS[current_idx]);
                display_show_text(display_buf);
                
                // Send a test IR command to verify the captured command
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

        // 3. Botton 2 (GPIO34): Save the captured command to NVS and enqueue it for MQTT
        if (gpio_get_level(BUTTON_2_GPIO) != 0) {
            if (has_cmd) {
                if (explorer_memory_save_ir(current_idx, &captured_cmd)) {
                    ESP_LOGI(TAG, "Comando [%s] gravado na NVS!", IR_ACTIONS[current_idx]);
                    
                    // Enqueue the learned IR command in JSON format for MQTT transmission
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

/**
 * @brief Task that consumes messages received via MQTT
 * 
 * @param pvParameters Pointer to the task parameters
 */
static void mqtt_consumer_task(void *pvParameters) {
    // Allocate the mqtt_message_t structure on the stack to avoid dynamic memory allocation
    static mqtt_message_t msg;
    int inactivity_counter = 0;
    
    ESP_LOGI(TAG, "Task consumidora MQTT iniciada com sucesso.");

    while (1) {
        // Wait for a message from the MQTT queue with a timeout of 1 second
        if (xQueueReceive(g_mqtt_queue, &msg, pdMS_TO_TICKS(MQTT_TX_QUEUE_RECEIVE_TIMOUT_TICKS)) == pdTRUE) {
            // Received a command: reset the inactivity counter
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

                                // Map the "action" string to the corresponding command index
                                if (strcmp(action_str, "LIGAR") == 0) {
                                    cmd_index = 0;
                                } else if (strcmp(action_str, "DESLIGAR") == 0) {
                                    cmd_index = 1;
                                } else if (strncmp(action_str, "SET_TEMP_", 9) == 0) {
                                    int temp = atoi(&action_str[9]); // Extract the numeric value after "SET_TEMP_"
                                    if (temp >= 18 && temp <= 25) {
                                        // Indices 2 to 9 map to temperatures from 18°C to 25°C
                                        cmd_index = 2 + (temp - 18);
                                    }
                                }

                                if (cmd_index >= 0) {
                                    ir_raw_command_t ir_cmd;
                                    if (explorer_memory_load_ir(cmd_index, &ir_cmd)) {
                                        ESP_LOGI(TAG, "Disparando comando IR [%s] (Índice: %d) via hardware...", action_str, cmd_index);
                                        
                                        // Trigger the IR command using the hardware
                                        ir_send_command(&ir_cmd);

                                        char disp_msg[64];
                                        snprintf(disp_msg, sizeof(disp_msg), "IR ENVIADO:\n%s", action_str);
                                        display_show_text(disp_msg);

                                        // Prepare and send the IR execution ACK (cmd_id: 4)
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
                                    // 1. Prepare the Wi-Fi ACK (cmd_id: 8)
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

                                        // 1. Prepare the Schedule ACK (cmd_id: 10)
                                        mqtt_message_t ack_msg;
                                        snprintf(ack_msg.payload, sizeof(ack_msg.payload),
                                                 "{\"cmd_id\":%d,\"status\":\"OK\",\"schedule_id\":%d}",
                                                 CMD_SET_SCHEDULE_ACK, sched_id);

                                        // 2. Enqueue the ACK message for MQTT transmission
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
            // Any packet received resets the inactivity counter, so if we reach here, it means no packet was received in the last second
            inactivity_counter++;
            ESP_LOGD(TAG, "Inatividade: %d/%d s", inactivity_counter, MQTT_INTERACTIVITY_TIMEOUT);

            if (inactivity_counter >= MQTT_INTERACTIVITY_TIMEOUT) {
                ESP_LOGW(TAG, "Tempo limite de inatividade (%d s) atingido. Encerrando task...", MQTT_INTERACTIVITY_TIMEOUT);
                display_clear();

                mqtt_stop();
                rtc_manager_process_schedules_and_sleep();
                vTaskDelete(NULL);
            }
        }
    }
}

/**
 * @brief Flushes the MQTT TX queue and sends all queued messages
 * 
 */
static void flush_mqtt_tx_queue(void) {
    mqtt_message_t tx_msg;
    int count = 0;

    while (xQueueReceive(g_mqtt_tx_queue, &tx_msg, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (mqtt_publish_commands_json(tx_msg.payload, NULL)) {
            count++;
        } else {
            ESP_LOGE(TAG, "Falha ao publicar comando IR via MQTT");
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/**
 * @brief build and enqueue the telemetry JSON message for MQTT transmission
 * @return true if the telemetry message was successfully enqueued, false otherwise
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

/**
 * @brief Callback function for handling Wi-Fi status changes
 * @param title The title of the status message
 * @param subtitle The subtitle of the status message
 */
static void on_wifi_status_change(const char *title, const char *subtitle) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%s\n%s", title, subtitle);
    display_show_text(buffer);
}

/**
 * @brief Main application function
 */
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

    // Learning mode trigger: if both buttons are pressed during boot, enter IR learning routine
    if (check_learning_mode_trigger()) {
        run_ir_learning_routine();
    }

    rtc_manager_init();
    bool isHourSync = check_and_run_schedules();
    xTaskCreate(schedule_checker_task, "schedule_checker_task", 3072, NULL, 4, NULL);

    if (!wifi_is_connected()) {
        // 3x try to connect to Wi-Fi
        wifi_connect_init(on_wifi_status_change);
    }

    if (wifi_is_connected()) {
        ESP_LOGI(TAG, "Wi-Fi OK, RSSI: %d dBm", wifi_get_rssi());
        display_show_text("WIFI OK\nCONECTANDO MQTT");

        if(isHourSync == false && rtc_manager_sync_ntp() == true)
        {
            check_and_run_schedules();
        }

        // 2. Try to connect to the MQTT broker with a maximum of 3 retries
        if (mqtt_init_with_retry(3)) {
            display_show_text("WIFI OK\nONLINE");

            // 3. Create the MQTT consumer task and the TX queue for sending messages
            g_mqtt_queue = xQueueCreate(10, sizeof(mqtt_message_t));
            if (g_mqtt_queue != NULL) {
                xTaskCreate(mqtt_consumer_task, "mqtt_consumer_task", 8192, NULL, 5, NULL);
            } else {
                ESP_LOGE(TAG, "Falha ao criar fila MQTT!");
            }

            // 4. Send the telemetry message to the broker
            send_telemetry();

        } else {
            // If the MQTT connection fails, display an error message and go to sleep
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
            // Flush the MQTT TX queue to send any pending messages
            flush_mqtt_tx_queue();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}