/**
 * @file main.c
 * @brief Main application entry point and Event-Driven FSM for explorerAirConditioner.
 * @author Embedded Software Architect
 * @date 2026-09-07
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "main.h"

#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"

#define GPIO_POWER_HOLD_PIN   GPIO_NUM_22
#define GPIO_BTN_MENU_SELECT  GPIO_NUM_5
#define GPIO_BTN_MENU_ENTER   GPIO_NUM_38

#define BUTTON_HOLD_DURATION_MS 2000
#define BUTTON_POLL_INTERVAL_MS 50

static esp_err_t init_boot_gpios(void);
static bool check_dual_button_hold(void);
static void force_sleep(void);

static const char *TAG = "MAIN_APP";

// 3. Declarar a fila global
static QueueHandle_t s_app_event_queue = NULL;

static void force_sleep(void)
{
    // rtc_ht8563_clear_flags();
    // rtc_ht8563_set_timer(10);
    // 1. Calcula o próximo evento (Telemetria vs Schedule) e seta o alarme AF no RTC HT8563
    power_manager_schedule_next_wakeup();

    // 2. Parada dos periféricos de rede
    board_mqtt_stop();
    board_wifi_stop();
    vTaskDelay(pdMS_TO_TICKS(100));

    // 3. Corta a energia do circuito
    ESP_LOGI(TAG, "Desligando alimentação via GPIO_POWER_HOLD_PIN...");
    gpio_set_level(GPIO_POWER_HOLD_PIN, 1);
}

/**
 * @brief Converte rtc_date_time_t para epoch time_t em segundos para cálculos
 */
static time_t rtc_to_epoch(const rtc_date_time_t *dt) {
    struct tm t = {
        .tm_sec  = dt->second,
        .tm_min  = dt->minute,
        .tm_hour = dt->hour,
        .tm_mday = dt->day,
        .tm_mon  = dt->month - 1,
        .tm_year = dt->year - 1900,
        .tm_isdst = -1
    };
    return mktime(&t);
}

/**
 * @brief Converte epoch time_t de volta para rtc_date_time_t
 */
static void epoch_to_rtc(time_t epoch, rtc_date_time_t *dt) {
    struct tm t;
    localtime_r(&epoch, &t);
    dt->second  = (uint8_t)t.tm_sec;
    dt->minute  = (uint8_t)t.tm_min;
    dt->hour    = (uint8_t)t.tm_hour;
    dt->day     = (uint8_t)t.tm_mday;
    dt->month   = (uint8_t)(t.tm_mon + 1);
    dt->year    = (uint16_t)(t.tm_year + 1900);
    dt->weekday = (uint8_t)t.tm_wday; // 0 = Domingo
}

/**
 * @brief Converte string "HH:MM" em minutos do dia (0 a 1439).
 *        Robusto contra caracteres invisíveis e sujeira de memória.
 */
static int time_str_to_minutes(const char *time_str) {
    if (time_str == NULL) return -1;
    
    int h = -1, m = -1;
    if (sscanf(time_str, "%d:%d", &h, &m) == 2) {
        if (h >= 0 && h < 24 && m >= 0 && m < 60) {
            return h * 60 + m;
        }
    }
    return -1;
}

esp_err_t power_manager_schedule_next_wakeup(void) {
    rtc_date_time_t current_dt;
    esp_err_t err = rtc_ht8563_get_time(&current_dt);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[TAREFA 3] Falha ao obter horario atual do RTC HT8563.");
        return err;
    }

    // 1. Limpa todas as flags de interrupção no RTC
    rtc_ht8563_clear_flags();

    // 2. Obter intervalo de telemetria
    uint16_t telemetry_interval_sec = 300;
    app_storage_get_telemetry_interval(&telemetry_interval_sec);

    time_t now_epoch = rtc_to_epoch(&current_dt);
    time_t telemetry_target_epoch = now_epoch + telemetry_interval_sec;

    // 3. Varrer agendamentos na FRAM
    time_t closest_schedule_epoch = 0;
    schedule_payload_t closest_schedule = {0};
    bool found_valid_schedule = false;

    for (uint8_t id = 0; id < MAX_SCHEDULE_ITEMS; id++) {
        schedule_payload_t sched;
        memset(&sched, 0, sizeof(schedule_payload_t));
        
        if (app_storage_get_schedule(id, &sched) != ESP_OK) {
            continue;
        }

        // Bit 0 = Enable Flag
        if ((sched.week_days & 0x01) == 0) {
            continue;
        }

        int sched_min = time_str_to_minutes(sched.time);
        if (sched_min < 0) {
            ESP_LOGW(TAG, "[TAREFA 3] Agendamento ID %d com formato de hora invalido: '%s'", id, sched.time);
            continue;
        }

        // Varrer os próximos 7 dias
        for (int day_offset = 0; day_offset < 7; day_offset++) {
            time_t candidate_day_epoch = now_epoch + (day_offset * 86400);
            struct tm tm_candidate;
            localtime_r(&candidate_day_epoch, &tm_candidate);

            // Bit 1 = Dom (tm_wday=0), Bit 2 = Seg (tm_wday=1), Bit 3 = Ter (tm_wday=2), ...
            uint8_t day_bit = (1 << (tm_candidate.tm_wday + 1));

            if (sched.week_days & day_bit) {
                tm_candidate.tm_hour = sched_min / 60;
                tm_candidate.tm_min  = sched_min % 60;
                tm_candidate.tm_sec  = 0;
                tm_candidate.tm_isdst = -1; // Força re-cálculo do Epoch sem fuso/DST manual

                time_t candidate_epoch = mktime(&tm_candidate);

                // Considera agendamento se for igual ou superior ao horário atual
                if (candidate_epoch >= now_epoch) {
                    if (!found_valid_schedule || candidate_epoch < closest_schedule_epoch) {
                        closest_schedule_epoch = candidate_epoch;
                        closest_schedule = sched;
                        found_valid_schedule = true;
                    }
                    break; 
                }
            }
        }
    }

    // 4. Tomada de Decisão (Tarefa 3)
    wakeup_context_t wakeup_ctx = {0};

    if (found_valid_schedule && (closest_schedule_epoch <= telemetry_target_epoch)) {
        rtc_date_time_t target_dt;
        epoch_to_rtc(closest_schedule_epoch, &target_dt);

        err = rtc_ht8563_set_alarm(target_dt.hour, target_dt.minute);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "[TAREFA 3] ALARME (AF) PRIORIZADO: Schedule ID=%d para %02d:%02d (em %ld s) | Acao IR: %d",
                     closest_schedule.schedule_id, target_dt.hour, target_dt.minute,
                     (long)(closest_schedule_epoch - now_epoch), closest_schedule.action);
        } else {
            ESP_LOGE(TAG, "[TAREFA 3] Erro ao gravar Alarme (AF) no RTC HT8563.");
        }

        wakeup_ctx.reason = WAKEUP_REASON_SCHEDULE;
        wakeup_ctx.schedule_id = closest_schedule.schedule_id;
        wakeup_ctx.pending_action = closest_schedule.action;

    } else {
        err = rtc_ht8563_set_timer(telemetry_interval_sec);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "[TAREFA 3] TIMER (TF) PRIORIZADO: Telemetria programada para daqui a %d s. (Proximo agendamento em: %ld s)", 
                     telemetry_interval_sec, 
                     found_valid_schedule ? (long)(closest_schedule_epoch - now_epoch) : -1L);
        } else {
            ESP_LOGE(TAG, "[TAREFA 3] Erro ao gravar Timer (TF) no RTC HT8563.");
        }

        wakeup_ctx.reason = WAKEUP_REASON_TELEMETRY;
        wakeup_ctx.schedule_id = 0xFF;
        wakeup_ctx.pending_action = LAST_ACTION_NONE;
    }

    // 5. Salva na FRAM
    app_storage_save_wakeup_context(&wakeup_ctx);

    return ESP_OK;
}

/**
 * @brief Handles incoming MQTT payloads and processes Command IDs using a switch statement.
 * 
 * @param payload Raw JSON payload received from MQTT Downlink topic.
 */
static void process_incoming_mqtt_command(const char *payload) {
    if (payload == NULL) {
        ESP_LOGE(TAG, "Null payload received in MQTT command processor");
        return;
    }

    int cmd_id = -1;
    esp_err_t err = json_get_cmd_id(payload, &cmd_id);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse 'cmd_id' from payload: %s", payload);
        return;
    }

    switch (cmd_id) {
        case CMD_ID_RTC_SYNC:
            ESP_LOGI(TAG, "[MQTT RX] Command 1 Received: Telemetry Ack / RTC Sync Payload");
            {
                cmd1_sync_data_t sync_data;
                if (json_decode_sync(payload, &sync_data) == ESP_OK)
                {
                    // 1. Lê a data e hora correntes salvas no RTC antes do update
                    rtc_date_time_t current_rtc = {0};
                    rtc_ht8563_get_time(&current_rtc);

                    // 2. Mescla os dados recebidos com a data existente no hardware
                    rtc_date_time_t dt = {
                        .hour = (uint8_t)sync_data.sync_time_t.hour,
                        .minute = (uint8_t)sync_data.sync_time_t.minute,
                        .second = (uint8_t)sync_data.sync_time_t.second,
                        .day = (sync_data.sync_time_t.day > 0) ? (uint8_t)sync_data.sync_time_t.day : current_rtc.day,
                        .month = (sync_data.sync_time_t.month > 0) ? (uint8_t)sync_data.sync_time_t.month : current_rtc.month,
                        .year = (sync_data.sync_time_t.year > 0) ? (uint16_t)sync_data.sync_time_t.year : current_rtc.year,
                        .weekday = (uint8_t)sync_data.sync_time_t.weekday,
                    };

                    // Sanidade para evitar gravação de ano 0 se o RTC estiver virgem
                    if (dt.year < 2026)
                        dt.year = 2026;
                    if (dt.day == 0)
                        dt.day = 1;
                    if (dt.month == 0)
                        dt.month = 1;

                    // 3. Recalcula o weekday exato com base na data (Ano-Mês-Dia) para evitar divergências
                    // struct tm tm_calc = {
                    //     .tm_sec = dt.second,
                    //     .tm_min = dt.minute,
                    //     .tm_hour = dt.hour,
                    //     .tm_mday = dt.day,
                    //     .tm_mon = dt.month - 1,
                    //     .tm_year = dt.year - 1900,
                    //     .tm_isdst = -1};
                    // time_t t_calc = mktime(&tm_calc);
                    // struct tm tm_out;
                    // localtime_r(&t_calc, &tm_out);

                    // // Atribui o dia da semana real calculado (0 = Dom, 1 = Seg, 2 = Ter...)
                    // dt.weekday = (uint8_t)tm_out.tm_wday;

                    // 3. Atualiza o RTC com a estrutura consistente
                    if (rtc_ht8563_set_time(&dt) == ESP_OK)
                    {
                        ESP_LOGI(TAG, "RTC atualizado com sucesso: %04d-%02d-%02d %02d:%02d:%02d (Dia da semana: %d)",
                                 dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second, dt.weekday);
                    }
                    else
                    {
                        ESP_LOGE(TAG, "Falha ao gravar no RTC HT8563");
                    }

                    // 2. Salvar o novo intervalo de telemetria na FRAM
                    app_storage_save_telemetry_interval((uint16_t)sync_data.telemetry_update);

                    // 3. Notificar a FSM que o timer foi atualizado para prosseguir com o encerramento
                    app_event_t evt = {
                        .type = APP_EVENT_TIMER_SET_SUCCESS};
                    xQueueSend(s_app_event_queue, &evt, portMAX_DELAY);
                }
                else
                {
                    ESP_LOGE(TAG, "Falha ao decodificar JSON do CMD 1 (RTC Sync)");
                }
            }
            break;

        case CMD_ID_GET_IR_LEARNED: // CMD 2
            ESP_LOGI(TAG, "[MQTT RX] Command 2 Received: Get IR Learned Queue Request");
            // TODO: Process request to send learned IR pulse data to platform
            break;

        case CMD_ID_WIFI_PROV: // CMD 4
            ESP_LOGI(TAG, "[MQTT RX] Command 4 Received: Wi-Fi Credentials Provisioning");
            {
                wifi_prov_payload_t wifi_payload;
                if (json_decode_wifi_prov(payload, &wifi_payload) == ESP_OK) {
                    // 1. Prepara dados para a FRAM
                    wifi_credentials_t new_creds = {0};
                    snprintf(new_creds.ssid, sizeof(new_creds.ssid), "%s", wifi_payload.ssid);
                    snprintf(new_creds.password, sizeof(new_creds.password), "%s", wifi_payload.password);
                    new_creds.is_valid = 1;

                    // 2. Persiste na FRAM
                    if (app_storage_save_wifi_credentials(&new_creds) == ESP_OK) {
                        // 3. Monta e envia a resposta de confirmação (CMD 5 - ACK)
                        char tx_ack_buf[256];
                        if (json_encode_wifi_ack(&wifi_payload, tx_ack_buf, sizeof(tx_ack_buf)) == ESP_OK) {
                            board_mqtt_publish_uplink(tx_ack_buf, 1);
                            ESP_LOGI(TAG, "[MQTT TX] CMD 5 (Wi-Fi ACK) enviado: %s", tx_ack_buf);
                        }
                    }
                } else {
                    ESP_LOGE(TAG, "Falha ao decodificar credenciais Wi-Fi do CMD 4");
                }
            }
            break;

        case CMD_ID_SCHEDULE_PROV: // CMD 6
            ESP_LOGI(TAG, "[MQTT RX] Command 6 Received: Schedule Provisioning");
            {
                schedule_payload_t sched_payload;
                if (json_decode_schedule(payload, &sched_payload) == ESP_OK) {
                    // 1. Salva a regra de agendamento na FRAM no ID correspondente (0-10)
                    if (app_storage_save_schedule(&sched_payload) == ESP_OK) {
                        // 2. Monta e responde a confirmação de recebimento (CMD 7 - ACK)
                        char tx_ack_buf[256];
                        if (json_encode_schedule_ack(&sched_payload, tx_ack_buf, sizeof(tx_ack_buf)) == ESP_OK) {
                            board_mqtt_publish_uplink(tx_ack_buf, 1);
                            ESP_LOGI(TAG, "[MQTT TX] CMD 7 (Schedule ACK) enviado: %s", tx_ack_buf);
                        }
                    }
                } else {
                    ESP_LOGE(TAG, "Falha ao decodificar agendamento do CMD 6");
                }
            }
            break;

        case CMD_ID_SET_IR_RAW_DATA: // CMD 8
            {
            ESP_LOGI(TAG, "[MQTT RX] Command 8 Received: Set IR Raw Data Payload");
            // TODO: Decode payload with and write waveforms to FRAM

            // Em vez de chamar force_sleep() diretamente:
            app_event_t evt = {
                .type = APP_EVENT_SHUTDOWN_REQUESTED};
            xQueueSend(s_app_event_queue, &evt, portMAX_DELAY);
            }
            break;

        default:
            ESP_LOGW(TAG, "[MQTT RX] Unhandled or Unknown Command ID Received: %d", cmd_id);
            break;
    }
}

/**
 * @brief Wi-Fi Event Handler bridging into the Central Application Queue.
 */
static void on_wifi_event_handler(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data) {
    app_event_t evt = {0};
    if (base == BOARD_WIFI_EVENTS) {
        if (id == BOARD_WIFI_EVENT_CONNECTED) {
            evt.type = APP_EVENT_WIFI_CONNECTED;
            xQueueSend(s_app_event_queue, &evt, portMAX_DELAY);
        } else if (id == BOARD_WIFI_EVENT_FAILOVER_EXHAUSTED) {
            evt.type = APP_EVENT_WIFI_FAILOVER_EXHAUSTED;
            xQueueSend(s_app_event_queue, &evt, portMAX_DELAY);
        }
    }
}

/**
 * @brief MQTT Event Handler bridging into the Central Application Queue.
 */
static void on_mqtt_event_handler(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data) {
    app_event_t evt = {0};
    if (base == BOARD_MQTT_EVENTS) {
        if (id == BOARD_MQTT_EVENT_CONNECTED) {
            evt.type = APP_EVENT_MQTT_CONNECTED;
            xQueueSend(s_app_event_queue, &evt, portMAX_DELAY);
        } else if (id == BOARD_MQTT_EVENT_DISCONNECTED) {
            evt.type = APP_EVENT_MQTT_DISCONNECTED;
            xQueueSend(s_app_event_queue, &evt, portMAX_DELAY);
        } else if (id == BOARD_MQTT_EVENT_DATA_RECEIVED) {
            if (event_data != NULL) {
                evt.type = APP_EVENT_MQTT_DATA_RECEIVED;
                memcpy(&evt.mqtt_data, event_data, sizeof(board_mqtt_data_t));
                xQueueSend(s_app_event_queue, &evt, portMAX_DELAY);
            }
        }
    }
}

/**
 * @brief Helper function to construct and send initial telemetry (CMD 0) with mocked data.
 * 
 * @return esp_err_t ESP_OK on successful MQTT publishing.
 */
static esp_err_t send_mocked_initial_telemetry(void) {
    ESP_LOGI(TAG, "Constructing CMD 0 (Initial Telemetry) with mocked data...");

    // 1. Preenche a estrutura de telemetria com dados mockados
    telemetry_data_t mock_telemetry = {
        .temp = 24,                 // 24 °C
        .umid = 58,                 // 58% de umidade
        .sync_time_t.hour = 14,     // 14h
        .sync_time_t.minute = 30,   // 30m
        .sync_time_t.second = 30,   // 30m
        .sync_time_t.day = 8,       // 8
        .sync_time_t.month = 9,     // Sep
        .sync_time_t.year = 2026,   // 2026
        .sync_time_t.weekday = 2,   // Thr
        .rssi = -65,                // -65 dBm
        .battery_mv = 3700,         // 3.7 V (3700 mV)
        .last_action = ACTION_SET_TEMP_24
    };

    // 2. Buffer para armazenar a payload JSON formatada
    char json_buffer[256];
    esp_err_t err = json_encode_telemetry(&mock_telemetry, json_buffer, sizeof(json_buffer));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao codificar JSON de telemetria: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Payload gerada: %s", json_buffer);

    // 3. Publica a payload via MQTT no tópico configurado
    err = board_mqtt_publish_uplink(json_buffer, 1);
    if (err == -1) {
        ESP_LOGE(TAG, "Falha ao publicar telemetria via MQTT: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Telemetria CMD 0 enviada com sucesso!");
    return ESP_OK;
}

/**
 * @brief Simulates dynamic Wi-Fi credential loading from FRAM.
 */
static esp_err_t setup_simulated_fram_wifi_credentials(void) {
    wifi_credentials_t fram_creds = {0};
    
    // Tenta buscar credenciais gravadas na FRAM
    esp_err_t err = app_storage_get_wifi_credentials(&fram_creds);
    if (err == ESP_OK) {
        wifi_credential_t cred = {0};
        snprintf(cred.ssid, sizeof(cred.ssid), "%s", fram_creds.ssid);
        snprintf(cred.password, sizeof(cred.password), "%s", fram_creds.password);

        ESP_LOGI(TAG, "Credencial do cliente carregada da FRAM: SSID='%s'", cred.ssid);
        return board_wifi_set_dynamic_credential(&cred);
    } else {
        ESP_LOGW(TAG, "Nenhuma credencial dinâmica de Wi-Fi encontrada na FRAM. Operando apenas com redes de fallback.");

        wifi_credential_t cred = {0};
        snprintf(cred.ssid, sizeof(cred.ssid), "SenFio3");
        snprintf(cred.password, sizeof(cred.password), "123456789");

        ESP_LOGI(TAG, "Dynamic Credential Loaded: SSID='%s'", cred.ssid);
        return board_wifi_set_dynamic_credential(&cred);
    }

    return ESP_OK;
}

/**
 * @brief Initializes all system hardware components, drivers, and buses.
 * 
 * @return esp_err_t ESP_OK on success, or an error code describing the failure.
 */
static esp_err_t board_hardware_init(void) {
    // Disable brownout detector during startup if supply fluctuates
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    // 1. Auto-Power Hold Circuit Initialization
    gpio_config_t pwr_conf = {
        .pin_bit_mask = (1ULL << GPIO_POWER_HOLD_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    esp_err_t ret = gpio_config(&pwr_conf);
    if (ret != ESP_OK) return ret;

    // 2. Initialize NVS Flash (Required by ESP-IDF Wi-Fi Stack)
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) return ret;

    // 3. Hardware Buses & Peripheral Drivers Initialization
    ret = board_i2c_bus_init();
    if (ret != ESP_OK) return ret;

    ret = rtc_ht8563_init();
    if (ret != ESP_OK) return ret;

    ret = app_storage_init();
    if (ret != ESP_OK) return ret;

    ret = oled_init(OLED_I2C_ADDR_DEFAULT);
    if (ret != ESP_OK) return ret;

    // 4. Initialize Wi-Fi Driver and Register Event Handlers
    ret = board_wifi_init();
    if (ret != ESP_OK) return ret;

    ret = esp_event_handler_instance_register(
        BOARD_WIFI_EVENTS, ESP_EVENT_ANY_ID, &on_wifi_event_handler, NULL, NULL);
    if (ret != ESP_OK) return ret;

    // 5. Load Wi-Fi Credentials
    ret = setup_simulated_fram_wifi_credentials();
    if (ret != ESP_OK) return ret;

    ESP_LOGI(TAG, "=== explorerAirConditioner Hardware System Initialized ===");
    return ESP_OK;
}

/**
 * @brief FreeRTOS Task executing the central application Finite State Machine (FSM).
 * 
 * @param pvParameters Unused task parameter pointer.
 */
static void app_fsm_task(void *pvParameters) {
    app_event_t current_evt;

    // Envia o evento inicial para disparar a sequencia
    boot_event_t boot_cause;
    
    // Executa a Análise da Tarefa 0
    if (analyze_boot_cause(&boot_cause) == ESP_OK) {
        app_event_t initial_evt = {
            .type = APP_EVENT_BOOT_ANALYZED,
            .boot_cause = boot_cause
        };
        xQueueSend(s_app_event_queue, &initial_evt, portMAX_DELAY);
    }

    while (1) {
        if (xQueueReceive(s_app_event_queue, &current_evt, portMAX_DELAY) == pdTRUE) {
            switch (current_evt.type) {

                case APP_EVENT_BOOT_ANALYZED:
                    ESP_LOGI(TAG, "[FSM] Boot Cause Analyzed. Starting Wi-Fi failover sequence...");
                    board_wifi_start_failover_connect();
                    break;

                case APP_EVENT_WIFI_CONNECTED:
                    ESP_LOGI(TAG, "[FSM] Wi-Fi Connected! Registering MQTT handlers & connecting to: %s", CONFIG_MQTT_BROKER_URI);
                    ESP_ERROR_CHECK(esp_event_handler_instance_register(
                        BOARD_MQTT_EVENTS, ESP_EVENT_ANY_ID, &on_mqtt_event_handler, NULL, NULL));

                    ESP_ERROR_CHECK(board_mqtt_init(
                        CONFIG_MQTT_BROKER_URI,
                        CONFIG_MQTT_BUFFER_SIZE,
                        CONFIG_MQTT_OUT_BUFFER_SIZE
                    ));
                    ESP_ERROR_CHECK(board_mqtt_start());
                    break;

                case APP_EVENT_WIFI_FAILOVER_EXHAUSTED:
                    ESP_LOGE(TAG, "[FSM] ERROR: Unable to connect to any known Wi-Fi network!");
                    break;

                case APP_EVENT_MQTT_CONNECTED:
                    ESP_LOGI(TAG, "[FSM] Connected to MQTT Broker successfully!");

                    vTaskDelay(pdMS_TO_TICKS(100));
                    if (send_mocked_initial_telemetry() == ESP_OK) {
                        ESP_LOGI(TAG, "[FSM] Telemetry CMD 0 successfully published");
                    } else {
                        ESP_LOGE(TAG, "[FSM] Failed to publish Telemetry CMD 0");
                    }
                    break;

                case APP_EVENT_MQTT_DATA_RECEIVED:
                    ESP_LOGI(TAG, "[FSM] MQTT Payload received on Topic: %s", current_evt.mqtt_data.topic);
                    process_incoming_mqtt_command(current_evt.mqtt_data.payload);
                    break;

                case APP_EVENT_MQTT_DISCONNECTED:
                    ESP_LOGW(TAG, "[FSM] Disconnected from MQTT Broker.");
                    break;

                case APP_EVENT_TIMER_SET_SUCCESS:
                    ESP_LOGI(TAG, "[FSM] Sequence Completed. Powering down circuit...");
                    force_sleep();
                    break;

                case APP_EVENT_SHUTDOWN_REQUESTED:
                    ESP_LOGI(TAG, "[FSM] Shutdown requested. Executing forced sleep routine...");
                    force_sleep();
                    break;

                default:
                    ESP_LOGW(TAG, "[FSM] Unhandled central application event: %d", current_evt.type);
                    break;
            }
        }
    }
}

/**
 * @brief Configures button GPIOs for initial reading during boot.
 */
static esp_err_t init_boot_gpios(void) {
    gpio_config_t btn_config = {
        .pin_bit_mask = (1ULL << GPIO_BTN_MENU_SELECT) | (1ULL << GPIO_BTN_MENU_ENTER),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    return gpio_config(&btn_config);
}

/**
 * @brief Checks if dual buttons (GPIO 5 and GPIO 38) are held for at least 2 seconds.
 * 
 * @return true if both buttons are held continuously for 2s, false otherwise.
 */
static bool check_dual_button_hold(void) {
    int elapsed_ms = 0;

    while (elapsed_ms < BUTTON_HOLD_DURATION_MS) {
        // Active LOW: 0 = Pressionado, 1 = Solto
        bool btn_select_pressed = (gpio_get_level(GPIO_BTN_MENU_SELECT) == 0);
        bool btn_enter_pressed  = (gpio_get_level(GPIO_BTN_MENU_ENTER)  == 0);

        // Se QUALQUER UM dos botões for solto durante a contagem, aborta
        if (btn_select_pressed || btn_enter_pressed) {
            return false;
        }

        vTaskDelay(pdMS_TO_TICKS(BUTTON_POLL_INTERVAL_MS));
        elapsed_ms += BUTTON_POLL_INTERVAL_MS;
    }

    return true;
}

// Função auxiliar para recuperar a ação/agendamento salvo da FRAM
static esp_err_t execute_pending_fram_action(void) {
    wakeup_context_t wakeup_ctx = {0};
    
    esp_err_t err = app_storage_get_wakeup_context(&wakeup_ctx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao ler contexto de wakeup da FRAM: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Contexto recuperado da FRAM - Razao: %d, ID: %d, Action Enum: %d", 
             wakeup_ctx.reason, wakeup_ctx.schedule_id, wakeup_ctx.pending_action);
    
    switch (wakeup_ctx.pending_action) {
        case LAST_ACTION_NONE:
            ESP_LOGI(TAG, "Nenhuma acao IR pendente (Apenas Telemetria).");
            break;
        case LAST_ACTION_LEARNED_ACK:
            ESP_LOGI(TAG, "Acao: LAST_ACTION_LEARNED_ACK");
            break;
        case ACTION_POWER_OFF:
            ESP_LOGI(TAG, "Acao: ACTION_POWER_OFF");
            break;
        case ACTION_POWER_ON:
            ESP_LOGI(TAG, "Acao: ACTION_POWER_ON");
            break;
        case ACTION_SET_TEMP_18:
            ESP_LOGI(TAG, "Acao: ACTION_SET_TEMP_18");
            break;
        case ACTION_SET_TEMP_19:
            ESP_LOGI(TAG, "Acao: ACTION_SET_TEMP_19");
            break;
        case ACTION_SET_TEMP_20:
            ESP_LOGI(TAG, "Acao: ACTION_SET_TEMP_20");
            break;
        case ACTION_SET_TEMP_21:
            ESP_LOGI(TAG, "Acao: ACTION_SET_TEMP_21");
            break;
        case ACTION_SET_TEMP_22:
            ESP_LOGI(TAG, "Acao: ACTION_SET_TEMP_22");
            break;
        case ACTION_SET_TEMP_23:
            ESP_LOGI(TAG, "Acao: ACTION_SET_TEMP_23");
            break;
        case ACTION_SET_TEMP_24:
            ESP_LOGI(TAG, "Acao: ACTION_SET_TEMP_24");
            break;
        case ACTION_SET_TEMP_25:
            ESP_LOGI(TAG, "Acao: ACTION_SET_TEMP_25");
            break;
        default:
            ESP_LOGW(TAG, "Acao IR desconhecida: %d", wakeup_ctx.pending_action);
            break;
    }

    return ESP_OK;
}

esp_err_t analyze_boot_cause(boot_event_t *out_event) {
    if (out_event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Configura os GPIOs dos botões com PULL-UP ativo
    init_boot_gpios();

    // Avaliação Active LOW (pino == 0 significa PRESSIONADO)
    bool btn_select_pressed = (gpio_get_level(GPIO_BTN_MENU_SELECT) == 0);
    bool btn_enter_pressed  = (gpio_get_level(GPIO_BTN_MENU_ENTER)  == 0);

    // 1. Ambas as teclas pressionadas simultaneamente no boot
    if (!btn_select_pressed && !btn_enter_pressed) {
        ESP_LOGI(TAG, "Dual buttons detected at boot. Checking 2s hold condition...");
        if (check_dual_button_hold()) {
            *out_event = EVENT_WAKEUP_BUTTON_DUAL_HOLD;
        } else {
            // Se soltar antes dos 2 segundos, trata como boot normal
            *out_event = EVENT_BOOT_POWER_ON;
        }
    } 
    // 2. Nenhum botão pressionado: Verificar RTC via I2C
    else {
        bool timer_flag = false;  // Periodic Telemetry Flag (TF)
        bool alarm_flag = false;  // Schedule Alarm Flag (AF)

        esp_err_t ret = rtc_ht8563_get_flags(&timer_flag, &alarm_flag);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read HT8563 RTC flags. Defaulting to Power-On Reset.");
            *out_event = EVENT_BOOT_POWER_ON;
            return ret;
        }

        if (timer_flag && alarm_flag) {
            *out_event = EVENT_WAKEUP_SCHEDULE_TELEMETRY_CONFLICT;
        } else if (timer_flag) {
            *out_event = EVENT_WAKEUP_RTC_TIMER;
        } else if (alarm_flag) {
            *out_event = EVENT_WAKEUP_RTC_ALARM;
        } else {
            *out_event = EVENT_BOOT_POWER_ON;
        }
    }

    // 3. Process Detected Event Action via Switch-Case
    switch (*out_event) {

        case EVENT_WAKEUP_SCHEDULE_TELEMETRY_CONFLICT:
        case EVENT_WAKEUP_RTC_ALARM:
        case EVENT_WAKEUP_RTC_TIMER:
        case EVENT_WAKEUP_SINGLE_BUTTON:
        case EVENT_BOOT_POWER_ON:
        case EVENT_LOW_BATTERY_SHUTDOWN:
        {
            ESP_LOGI(TAG, "[BOOT CAUSE] Hard Reset / Power-On Reset or RTC detected.");
            // TODO: Initialize system defaults, perform RTC sanity check, and check FRAM state.
            execute_pending_fram_action();
        }
            break;

        case EVENT_WAKEUP_BUTTON_DUAL_HOLD:
            ESP_LOGI(TAG, "[BOOT CAUSE] Dual Button Hold (>= 2s) confirmed. Triggering IR Learn/Test Mode.");
            // TODO: Initialize OLED display menu with "Aprender" and "Testar" options.
            break;

        default:
            ESP_LOGW(TAG, "[BOOT CAUSE] Evento de boot nao mapeado: %d", *out_event);
            break;
    }

    return ESP_OK;
}

void app_main(void) {
    // 1. Criar a fila de eventos
    s_app_event_queue = xQueueCreate(10, sizeof(app_event_t));
    if (s_app_event_queue == NULL) {
        ESP_LOGE(TAG, "Critical: Failed to create central application event queue");
        return;
    }

    // 2. Inicializar Todo o Hardware (GPIOs, I2C, RTC, etc.) PRIMEIRO!
    ESP_ERROR_CHECK(board_hardware_init());

    // 3. Criar a tarefa da FSM somente após o hardware estar pronto
    BaseType_t ret = xTaskCreate(
        app_fsm_task,
        "app_fsm_task",
        8192,
        NULL,
        5,
        NULL
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Critical: Failed to create FSM task");
    }
}