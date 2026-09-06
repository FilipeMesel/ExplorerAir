#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "sdkconfig.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"

#include "fram_mb85rs512t.h"

#include "board_i2c_bus.h"
#include "display_oled.h"
#include "rtc_ht8563.h"

#include "board_wifi.h"
#include "board_mqtt.h"

#include "json_protocol.h"

static const char *TAG = "MAIN_APP";

static void system_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == BOARD_WIFI_EVENTS) {
        if (event_id == BOARD_WIFI_EVENT_CONNECTED) {
            ESP_LOGI(TAG, "Wi-Fi OK. Conectando ao MQTT...");
            board_mqtt_start();
        } else if (event_id == BOARD_WIFI_EVENT_FAILOVER_EXHAUSTED) {
            ESP_LOGE(TAG, "Excedeu tentativas de Wi-Fi! Exibindo no OLED e indo para Sleep...");
            //TODO: Show error on OLED and go to deep sleep
        }
    } else if (event_base == BOARD_MQTT_EVENTS) {
        if (event_id == BOARD_MQTT_EVENT_CONNECTED)
        {
            ESP_LOGI(TAG, "MQTT OK. Enviando Telemetria (CMD 0)...");
            char *json_payload = build_telemetry_json(24, 60, "10:00", "-10", 3.7f, ACTION_TELEMETRY);
            if (json_payload != NULL)
            {
                board_mqtt_publish_uplink(json_payload, 1);
                free(json_payload);
            }
            else
            {
                ESP_LOGE(TAG, "Falha ao gerar JSON de telemetria!");
            }
        }
        else if (event_id == BOARD_MQTT_EVENT_DATA_RECEIVED)
        {
            board_mqtt_data_t *msg = (board_mqtt_data_t *)event_data;
            int cmd_id = -1;
            if (json_get_cmd_id(msg->payload, &cmd_id) != ESP_OK)
            {
                ESP_LOGE(TAG, "JSON recebido com formato ou cmd_id invalido");
                return;
            }

            switch (cmd_id)
            {
            case CMD_ID_RTC_SYNC:
            { // Sync RTC
                cmd1_rtc_sync_payload_t rtc_payload;
                if (json_decode_cmd1_rtc_sync(msg->payload, &rtc_payload) == ESP_OK)
                {
                    ESP_LOGI(TAG, "CMD 1 Recebido: Hora %02d:%02d:%02d | Intervalo Telemetria: %ds",
                             rtc_payload.hour, rtc_payload.minute, rtc_payload.second,
                             rtc_payload.interval_sec);
                    
                    // TODO: Atualizar registradores de hora do RTC HT8563 e o timer de telemetria
                } else {
                    ESP_LOGE(TAG, "Falha ao parsear payload do CMD 1 (RTC Sync)");
                }
                break;
            }
            case CMD_ID_WIFI_PROV:
            { // Wi-Fi provisioning
                cmd3_wifi_prov_payload_t wifi_payload;
                if (json_decode_cmd3_wifi_prov(msg->payload, &wifi_payload) == ESP_OK)
                {
                    ESP_LOGI(TAG, "CMD 3 Recebido: Novas credenciais Wi-Fi -> SSID: %s", wifi_payload.ssid);
                    
                    // TODO: Store it in FRAM

                    // Publish the ACK Wi-Fi (CMD 4)
                    wifi_ack_payload_t wifi_ack = {
                        .ssid = wifi_payload.ssid,
                        .password = wifi_payload.password
                    };

                    char *ack_json = NULL;
                    if (json_encode_wifi_ack(&wifi_ack, &ack_json) == ESP_OK && ack_json != NULL) {
                        board_mqtt_publish_uplink(ack_json, 1);
                        free(ack_json);
                    } else {
                        ESP_LOGE(TAG, "Falha ao gerar ACK (CMD 4) de Wi-Fi!");
                    }
                } else {
                    ESP_LOGE(TAG, "Falha ao parsear payload do CMD 3 (Wi-Fi Prov)");
                }
                break;
            }
            case CMD_ID_SCHEDULE_PROV:
            { // Schedule provisioning
                cmd5_schedule_payload_t sched_payload;
                if (json_decode_cmd5_schedule(msg->payload, &sched_payload) == ESP_OK)
                {
                    ESP_LOGI(TAG, "CMD 5 Recebido: %d agendamentos parseados com sucesso", sched_payload.count);
                    
                    // TODO: Add it in the Ring buffer from FRAM

                    // Send the ACK (CMD 6) for each schedule received in the array
                    for (int i = 0; i < sched_payload.count; i++) {
                        schedule_ack_payload_t sched_ack = {
                            .week_days = sched_payload.items[i].week_days,
                            .time = sched_payload.items[i].time,
                            .action = sched_payload.items[i].action,
                            .status = "OK"
                        };

                        char *ack_json = NULL;
                        if (json_encode_schedule_ack(&sched_ack, &ack_json) == ESP_OK && ack_json != NULL) {
                            board_mqtt_publish_uplink(ack_json, 1);
                            free(ack_json);
                        } else {
                            ESP_LOGE(TAG, "Falha ao gerar ACK (CMD 6) para o item %d!", i);
                        }
                    }
                } else {
                    ESP_LOGE(TAG, "Falha ao parsear payload do CMD 5 (Schedule Prov)");
                }
                break;
            }
            case CMD_ID_SET_IR_RAW_DATA:
            { // CMD 7: Set IR Raw Data
                cmd7_ir_raw_payload_t ir_payload;
                if (json_decode_cmd7_ir_raw(msg->payload, &ir_payload) == ESP_OK)
                {
                    ESP_LOGI(TAG, "CMD 7 Recebido: Frequencia %u Hz | Timings recebidos: %u",
                             ir_payload.frequency_hz, ir_payload.timings_count);

                    // TODO: Save the IR raw data in FRAM and send the ACK (CMD 8) with the count of valid timings received
                    
                    // Send the ACK CMD 8 (ACK)
                    cmd8_ir_raw_ack_payload_t ack = {
                        .status = "OK",
                        .count_received = ir_payload.timings_count
                    };
                    
                    char *ack_json = NULL;
                    if (json_encode_ir_raw_ack(&ack, &ack_json) == ESP_OK && ack_json != NULL) {
                        board_mqtt_publish_uplink(ack_json, 1);
                        free(ack_json);
                    } else {
                        ESP_LOGE(TAG, "Falha ao gerar ACK (CMD 8) de IR Raw!");
                    }
                } else {
                    ESP_LOGE(TAG, "Falha ao parsear payload do CMD 7 (IR Raw)");
                }
                break;
            }
            default:
                ESP_LOGW(TAG, "Comando Downlink nao reconhecido: CMD %d", cmd_id);
                break;
            }
        }
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Iniciando aplicação...");

    //-----------------------------------
    // Initialize the FRAM device
    //-------------------------------------
    ESP_ERROR_CHECK(fram_init());

    //-----------------------------------
    // Initialize the I2C bus for the OLED display and RTC
    //-----------------------------------
    ESP_ERROR_CHECK(board_i2c_bus_init());

    //-----------------------------------
    // Initialize the RTC device
    //-----------------------------------
    ESP_ERROR_CHECK(rtc_ht8563_init());

    //-----------------------------------
    // Initialize the OLED display with default I2C address
    //-----------------------------------
    ESP_ERROR_CHECK(oled_init(OLED_I2C_ADDR_DEFAULT));

    //-----------------------------------
    // Initialize Wi-Fi and MQTT subsystems
    //-----------------------------------

    /**
     * @brief Initialize NVS flash storage
     *
     * It's necessary to initialize NVS before using Wi-Fi or MQTT, as they may rely on stored credentials or configurations.
     * 
     */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    //-----------------------------------
    // Register event handlers for Wi-Fi and MQTT events
    //-----------------------------------
    esp_event_handler_instance_register(BOARD_WIFI_EVENTS, ESP_EVENT_ANY_ID, &system_event_handler, NULL, NULL);
    esp_event_handler_instance_register(BOARD_MQTT_EVENTS, ESP_EVENT_ANY_ID, &system_event_handler, NULL, NULL);

    //-----------------------------------------------
    // Initialize Wi-Fi and MQTT
    //-----------------------------------------------
    ESP_ERROR_CHECK(board_wifi_init());
    ESP_ERROR_CHECK(board_mqtt_init(CONFIG_MQTT_BROKER_URI, 
                                    CONFIG_MQTT_BUFFER_SIZE, 
                                    CONFIG_MQTT_OUT_BUFFER_SIZE));

    //-----------------------------------------------
    // Set dynamic Wi-Fi credentials and start failover connection
    //-----------------------------------------------
    // TODO: In a real application, these credentials would be read from FRAM.
    wifi_credential_t dynamic_cred = {.ssid = "SEU WIFI", .password = "SUA SENHA"};
    board_wifi_set_dynamic_credential(&dynamic_cred);

    board_wifi_start_failover_connect();

#if CONFIG_ENABLE_FRAM_TESTS
    fram_run_tests();
#endif

#if CONFIG_OLED_RUN_TESTS
    oled_run_tests();
#endif

#if RTC_HT8563_TEST_READ_DATETIME
    rtc_ht8563_run_configured_tests();
#endif

#if CONFIG_RTC_HT8563_TEST_ALARM_INT || CONFIG_RTC_HT8563_TEST_TIMER_INT
    rtc_ht8563_run_configured_tests();
    ESP_LOGI(TAG, "Aguardando desligamento...");
    vTaskDelay(pdMS_TO_TICKS(2000));

    // 1. Remove os dispositivos do barramento
    rtc_ht8563_deinit();
    oled_deinit();

    // 2. Agora sim encerra o barramento I2C com segurança
    board_i2c_bus_deinit();
#endif

    while (1) {
        
        ESP_LOGI(TAG, "Hello, World!");

        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second
    }

}