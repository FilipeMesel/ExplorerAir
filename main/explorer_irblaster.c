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
#include "explorer_protocol.h"

static const char *TAG = "EXPLORER_MAIN";
#define NVS_IR_NAMESPACE "ir_codes"

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
 * Converte um comando IR capturado para o JSON no formato esperado
 * e enfileira na g_mqtt_tx_queue.
 */
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
                        ESP_LOGI(TAG, "Tratando CMD_EXECUTE_IR");
                        display_show_text("CMD: EXECUTAR IR");
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
                        ESP_LOGI(TAG, "Tratando CMD_SET_WIFI");
                        display_show_text("CMD: CONFIG WIFI");
                        break;

                    case CMD_SET_WIFI_ACK:
                        ESP_LOGI(TAG, "Tratando CMD_SET_WIFI_ACK");
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
                display_show_text("MQTT TIMEOUT\nTASK ENCERRADA");
                
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

    ESP_LOGI(TAG, "Esvaziando fila de comandos aprendidos para publicação...");

    while (xQueueReceive(g_mqtt_tx_queue, &tx_msg, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (mqtt_publish_commands_json(tx_msg.payload, NULL)) {
            count++;
            ESP_LOGI(TAG, "Comando IR publicado via MQTT com sucesso (%d)", count);
        } else {
            ESP_LOGE(TAG, "Falha ao publicar comando IR via MQTT");
        }
        vTaskDelay(pdMS_TO_TICKS(200)); // Pequeno delay entre publicações
    }

    ESP_LOGI(TAG, "Total de comandos IR enviados ao broker: %d", count);
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
            mqtt_publish_status("{\"status\":\"online\"}", true);

            // 3. Cria Fila e inicia a Task consumidora dos comandos
            g_mqtt_queue = xQueueCreate(10, sizeof(mqtt_message_t));
            if (g_mqtt_queue != NULL) {
                xTaskCreate(mqtt_consumer_task, "mqtt_consumer_task", 8192, NULL, 5, NULL);
            } else {
                ESP_LOGE(TAG, "Falha ao criar fila MQTT!");
            }

            // 4. Envia todos os comandos aprendidos via MQTT
            flush_mqtt_tx_queue();

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
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}