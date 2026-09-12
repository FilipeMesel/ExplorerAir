#include "services/app_comms.h"
#include <string.h>
#include "esp_log.h"
#include "board_wifi.h"
#include "board_mqtt.h"
#include "app_storage.h"
#include "json_protocol.h"
#include "app_ui.h"
#include "rtc_ht8563.h"
#include "app_events.h"

static const char *TAG = "APP_COMMS";

esp_err_t app_comms_get_wifi_credentials_from_fram(void) {
    wifi_credentials_t fram_creds = {0};
    
    esp_err_t err = app_storage_get_wifi_credentials(&fram_creds);
    
    if (err == ESP_OK && strlen(fram_creds.ssid) > 0) {
        wifi_credential_t cred = {0};
        snprintf(cred.ssid, sizeof(cred.ssid), "%s", fram_creds.ssid);
        snprintf(cred.password, sizeof(cred.password), "%s", fram_creds.password);

        ESP_LOGI(TAG, "Credencial do cliente carregada da FRAM: SSID='%s'", cred.ssid);
        return board_wifi_set_dynamic_credential(&cred);
    } 
    
    ESP_LOGW(TAG, "Nenhuma credencial encontrada na FRAM. Usando fallback padrao.");
    wifi_credential_t fallback_cred = {0};
    snprintf(fallback_cred.ssid, sizeof(fallback_cred.ssid), "SenFio3");
    snprintf(fallback_cred.password, sizeof(fallback_cred.password), "123456789");

    ESP_LOGI(TAG, "Dynamic Credential Fallback Loaded: SSID='%s'", fallback_cred.ssid);
    return board_wifi_set_dynamic_credential(&fallback_cred);
}

static last_action_t get_last_action_from_fram(void) {
    wakeup_context_t wakeup_ctx = {0};
    esp_err_t err = app_storage_get_wakeup_context(&wakeup_ctx);
    
    if (err == ESP_OK) {
        return wakeup_ctx.pending_action;
    }
    
    ESP_LOGW(TAG, "Não foi possível ler wakeup context. Usando LAST_ACTION_NONE.");
    return LAST_ACTION_NONE;
}

void app_comms_on_wifi_event(void *handler_args, esp_event_base_t base, int32_t id, void *data) {
    app_event_t evt = {0};

    if (base == BOARD_WIFI_EVENTS) {
        switch (id) {
            case BOARD_WIFI_EVENT_CONNECTED:
                ESP_LOGI(TAG, "Wi-Fi conectado com sucesso.");
                evt.type = APP_EVENT_WIFI_CONNECTED;
                if (g_app_event_queue) {
                    xQueueSend(g_app_event_queue, &evt, 0);
                }
                break;

            case BOARD_WIFI_EVENT_FAILOVER_EXHAUSTED:
                ESP_LOGW(TAG, "Todas as tentativas de conexão Wi-Fi falharam (Failover Esgotado).");
                evt.type = APP_EVENT_WIFI_FAILOVER_EXHAUSTED;
                if (g_app_event_queue) {
                    xQueueSend(g_app_event_queue, &evt, 0);
                }
                break;
            
            case BOARD_WIFI_EVENT_DISCONNECTED:
                ESP_LOGW(TAG, "WiFi Desconectado");
                evt.type = APP_EVENT_SHUTDOWN_REQUESTED;
                if (g_app_event_queue) {
                    xQueueSend(g_app_event_queue, &evt, 0);
                }
                break;

            default:
                break;
        }
    }
}

void app_comms_on_mqtt_event(void *handler_args, esp_event_base_t base, int32_t id, void *data) {
    app_event_t evt = {0};

    if (base == BOARD_MQTT_EVENTS) {
        switch (id) {
            case BOARD_MQTT_EVENT_CONNECTED:
                ESP_LOGI(TAG, "MQTT conectado com sucesso.");
                evt.type = APP_EVENT_MQTT_CONNECTED;
                if (g_app_event_queue) {
                    xQueueSend(g_app_event_queue, &evt, 0);
                }
                break;

            case BOARD_MQTT_EVENT_DISCONNECTED:
                ESP_LOGW(TAG, "MQTT desconectado.");
                evt.type = APP_EVENT_MQTT_DISCONNECTED;
                if (g_app_event_queue) {
                    xQueueSend(g_app_event_queue, &evt, 0);
                }
                break;

            case BOARD_MQTT_EVENT_DATA_RECEIVED:
                if (data != NULL) {
                    evt.type = APP_EVENT_MQTT_DATA_RECEIVED;
                    memcpy(&evt.mqtt_data, data, sizeof(board_mqtt_data_t));
                    if (g_app_event_queue) {
                        xQueueSend(g_app_event_queue, &evt, 0);
                    }
                }
                break;

            default:
                break;
        }
    }
}

esp_err_t app_comms_process_mqtt_command(const char *json_str) {
    if (!json_str) return ESP_ERR_INVALID_ARG;

    int cmd_id = -1;
    esp_err_t err = json_get_cmd_id(json_str, &cmd_id);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao extrair cmd_id do payload JSON");
        return err;
    }

    ESP_LOGI(TAG, "Comando MQTT recebido com cmd_id: %d", cmd_id);

    switch (cmd_id) {
        case CMD_ID_RTC_SYNC: { // CMD 1: Sync RTC & Telemetry Interval
            cmd1_sync_data_t sync_data = {0};
            if (json_decode_sync(json_str, &sync_data) == ESP_OK) {
                // Preserva os dados do RTC anterior para fazer o merge de data/hora
                rtc_date_time_t current_rtc = {0};
                rtc_ht8563_get_time(&current_rtc);

                rtc_date_time_t dt = {
                    .hour    = (uint8_t)sync_data.sync_time_t.hour,
                    .minute  = (uint8_t)sync_data.sync_time_t.minute,
                    .second  = (uint8_t)sync_data.sync_time_t.second,
                    .day     = (sync_data.sync_time_t.day > 0) ? (uint8_t)sync_data.sync_time_t.day : current_rtc.day,
                    .month   = (sync_data.sync_time_t.month > 0) ? (uint8_t)sync_data.sync_time_t.month : current_rtc.month,
                    .year    = (sync_data.sync_time_t.year > 0) ? (uint16_t)sync_data.sync_time_t.year : current_rtc.year,
                    .weekday = (uint8_t)sync_data.sync_time_t.weekday,
                };

                if (dt.year < 2026) dt.year = 2026;
                if (dt.day == 0)    dt.day = 1;
                if (dt.month == 0)  dt.month = 1;

                if (rtc_ht8563_set_time(&dt) == ESP_OK) {
                    ESP_LOGI(TAG, "RTC atualizado com sucesso: %04d-%02d-%02d %02d:%02d:%02d",
                             dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
                }

                app_storage_save_telemetry_interval((uint16_t)sync_data.telemetry_update);
            } else {
                ESP_LOGE(TAG, "Falha ao decodificar JSON do CMD 1 (RTC Sync)");
            }
            break;
        }

        case CMD_ID_GET_IR_LEARNED: // CMD 2
            ESP_LOGI(TAG, "[MQTT RX] Command 2 Received: Get IR Learned Queue Request");
            break;

        case CMD_ID_WIFI_PROV: { // CMD 4: Wi-Fi Provisioning
            wifi_prov_payload_t prov = {0};
            if (json_decode_wifi_prov(json_str, &prov) == ESP_OK) {
                wifi_credentials_t creds = {0};
                strncpy(creds.ssid, prov.ssid, sizeof(creds.ssid) - 1);
                strncpy(creds.password, prov.password, sizeof(creds.password) - 1);
                creds.is_valid = 1;

                if (app_storage_save_wifi_credentials(&creds) == ESP_OK) {
                    char ack_buf[256] = {0};
                    if (json_encode_wifi_ack(&prov, ack_buf, sizeof(ack_buf)) == ESP_OK) {
                        board_mqtt_publish_uplink(ack_buf, 1); // QoS 1
                        ESP_LOGI(TAG, "[MQTT TX] CMD 5 (Wi-Fi ACK) enviado: %s", ack_buf);
                    }

                    app_ui_post_message("NOVO WIFI", "ATUALIZADO", 100);
                }
            } else {
                ESP_LOGE(TAG, "Falha ao decodificar credenciais Wi-Fi do CMD 4");
            }
            break;
        }

        case CMD_ID_SCHEDULE_PROV: { // CMD 6: Schedule Setting
            schedule_payload_t sched = {0};
            if (json_decode_schedule(json_str, &sched) == ESP_OK) {
                if (app_storage_save_schedule(&sched) == ESP_OK) {
                    char ack_buf[256] = {0};
                    if (json_encode_schedule_ack(&sched, ack_buf, sizeof(ack_buf)) == ESP_OK) {
                        board_mqtt_publish_uplink(ack_buf, 1); // QoS 1
                        ESP_LOGI(TAG, "[MQTT TX] CMD 7 (Schedule ACK) enviado: %s", ack_buf);

                        app_ui_post_message("NOVO AGENDAMENTO", "SALVO!", 100);
                    }
                }
            } else {
                ESP_LOGE(TAG, "Falha ao decodificar agendamento do CMD 6");
            }
            break;
        }

        case CMD_ID_SET_IR_RAW_DATA: { // CMD 8
            ESP_LOGI(TAG, "[MQTT RX] Command 8 Received: Set IR Raw Data Payload");

            app_ui_post_message("COMANDO IR", "ATUALIZADO", 100);
            break;
        }

        default:
            ESP_LOGW(TAG, "cmd_id %d nao tratado", cmd_id);
            break;
    }

    return ESP_OK;
}

// Descarrega todas as telemetrias armazenadas na FRAM via MQTT
esp_err_t app_comms_flush_offline_telemetries(void) {
    uint16_t pending_count = 0;
    app_storage_get_telemetry_log_count(&pending_count);

    if (pending_count == 0) {
        ESP_LOGI(TAG, "Nenhuma telemetria offline pendente na FRAM.");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Enviando %u telemetria(s) pendente(s) da FRAM...", pending_count);

    telemetry_data_t offline_item;
    char pub_buf[300];

    while (app_storage_pop_telemetry_log(&offline_item) == ESP_OK) {
        memset(pub_buf, 0, sizeof(pub_buf));
        
        if (json_encode_telemetry(&offline_item, pub_buf, sizeof(pub_buf)) == ESP_OK) {
            int msg_id = board_mqtt_publish_uplink(pub_buf, 1);
            if (msg_id < 0) {
                // Se falhar a publicação, devolve o item para a FRAM para não perder os dados
                ESP_LOGE(TAG, "Falha no envio MQTT do log offline. Reenfileirando...");
                app_storage_push_telemetry_log(&offline_item);
                return ESP_FAIL;
            }
            ESP_LOGI(TAG, "Telemetria offline enviada com sucesso.");
            vTaskDelay(pdMS_TO_TICKS(100)); // Pequeno delay para evitar sobrecarga na rede
        }
    }

    return ESP_OK;
}

// Atualização da função de envio inicial
esp_err_t app_comms_send_initial_telemetry(void) {

    int current_rssi = 0;
    board_wifi_get_rssi(&current_rssi);

    telemetry_data_t telemetry = {
        .temp = 24,
        .umid = 58,
        .rssi = current_rssi,
        .battery_mv = 3700,
        .last_action = get_last_action_from_fram()
    };

    rtc_ht8563_get_time(&telemetry.sync_time_t);

    char pub_buf[300] = {0};
    esp_err_t err = json_encode_telemetry(&telemetry, pub_buf, sizeof(pub_buf));
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Enviando telemetria atual via MQTT...");
        int msg_id = board_mqtt_publish_uplink(pub_buf, 1);
        
        if (msg_id < 0) {
            ESP_LOGW(TAG, "Falha ao enviar telemetria atual. Salvando na FRAM...");
            app_storage_push_telemetry_log(&telemetry);
            return ESP_FAIL;
        }

        // Se enviou a telemetria atual com sucesso, descarrega a fila acumulada na FRAM
        app_comms_flush_offline_telemetries();
        return ESP_OK;
    }

    return err;
}

esp_err_t app_comms_init(void) {
    // Inicialização do cliente MQTT com as configurações da SDK
    esp_err_t err = board_mqtt_init(
        CONFIG_MQTT_BROKER_URI,
        CONFIG_MQTT_BUFFER_SIZE,
        CONFIG_MQTT_OUT_BUFFER_SIZE
    );
    if (err != ESP_OK) return err;

    err = esp_event_handler_instance_register(BOARD_WIFI_EVENTS,
                                               ESP_EVENT_ANY_ID,
                                               app_comms_on_wifi_event,
                                               NULL,
                                               NULL);
    if (err != ESP_OK) return err;

    err = esp_event_handler_instance_register(BOARD_MQTT_EVENTS,
                                               ESP_EVENT_ANY_ID,
                                               app_comms_on_mqtt_event,
                                               NULL,
                                               NULL);
    if (err != ESP_OK) return err;

    ESP_LOGI(TAG, "Serviço app_comms inicializado com sucesso.");
    return ESP_OK;
}

esp_err_t app_comms_wifi_start_failover(void) {
    app_comms_get_wifi_credentials_from_fram();
    return board_wifi_start_failover_connect();
}