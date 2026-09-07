/**
 * @file main.c
 * @brief Main application entry point and Event-Driven FSM for explorerAirConditioner.
 * @author Embedded Software Architect
 * @date 2026-09-07
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "main.h"
#include "fram_mb85rs512t.h"
#include "board_i2c_bus.h"
#include "rtc_ht8563.h"
#include "display_oled.h"
#include "board_wifi.h"
#include "board_mqtt.h"
#include "json_protocol.h"

#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"

#define ESP_REG_GPIO 22

// Fallbacks de compilação caso as macros do Kconfig não estejam visíveis
#ifndef CONFIG_MQTT_BROKER_URI
#define CONFIG_MQTT_BROKER_URI "mqtt://broker.hivemq.com:1883"
#endif

#ifndef CONFIG_MQTT_BUFFER_SIZE
#define CONFIG_MQTT_BUFFER_SIZE 2048
#endif

#ifndef CONFIG_MQTT_OUT_BUFFER_SIZE
#define CONFIG_MQTT_OUT_BUFFER_SIZE 2048
#endif

static const char *TAG = "MAIN_APP";

/**
 * @brief System Unified Application Events for Main Central Queue
 */
typedef enum {
    APP_EVENT_BOOT_ANALYZED,
    APP_EVENT_WIFI_CONNECTED,
    APP_EVENT_WIFI_FAILOVER_EXHAUSTED,
    APP_EVENT_MQTT_CONNECTED,
    APP_EVENT_MQTT_DISCONNECTED,
    APP_EVENT_MQTT_DATA_RECEIVED,
    APP_EVENT_TIMER_SET_SUCCESS
} app_event_type_t;


// 2. Definir a struct do evento que faltava
typedef struct {
    app_event_type_t type;
    boot_event_t boot_cause;
    board_mqtt_data_t mqtt_data; // <--- Alterado de board_mqtt_event_data_t para board_mqtt_data_t
} app_event_t;

// 3. Declarar a fila global
static QueueHandle_t s_app_event_queue = NULL;

static void force_sleep()
{
    // Habilita a interrupção por timer de 10 segundos no RTC
    // 3. Limpa flags residuais e reseta interrupções pendentes do RTC
    rtc_ht8563_clear_flags();

    // 4. Configura a hora inicial do RTC para 00:01:50
    rtc_date_time_t dt_initial = {
        .second = 50,
        .minute = 1,
        .hour = 0,
        .day = 1,
        .weekday = 1,
        .month = 1,
        .year = 2026};

    if (rtc_ht8563_set_time(&dt_initial) == ESP_OK)
    {
        ESP_LOGI(TAG, "Hora inicial ajustada para: 00:01:50");
    }
    else
    {
        ESP_LOGE(TAG, "Falha ao definir hora inicial no RTC");
    }

    // 5. Configura o Alarme do HT8563 para disparar às 00:02:00
    // rtc_ht8563_set_alarm(hour, minute)
    if (rtc_ht8563_set_alarm(0, 2) == ESP_OK)
    {
        ESP_LOGI(TAG, "Alarme programado com sucesso para 00:02:00");
        ESP_LOGI(TAG, "[RTC] Timer de 10s configurado com sucesso!");
    }
    else
    {
        ESP_LOGE(TAG, "Falha ao configurar alarme no RTC");
    }

    // 2. Encerra periféricos e rede
    board_wifi_stop();
    vTaskDelay(pdMS_TO_TICKS(100));

    // 3. Corta a energia
    gpio_set_level(ESP_REG_GPIO, 1);
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
        case CMD_ID_RTC_SYNC: // CMD 1
            ESP_LOGI(TAG, "[MQTT RX] Command 1 Received: Telemetry Ack / RTC Sync Payload");
            // TODO: Decode payload with json_decode_cmd1_rtc_sync and update RTC / Timer interval
            break;

        case CMD_ID_GET_IR_LEARNED: // CMD 2
            ESP_LOGI(TAG, "[MQTT RX] Command 2 Received: Get IR Learned Queue Request");
            // TODO: Process request to send learned IR pulse data to platform
            break;

        case CMD_ID_WIFI_PROV: // CMD 4
            ESP_LOGI(TAG, "[MQTT RX] Command 4 Received: Wi-Fi Credentials Provisioning");
            // TODO: Decode payload with json_decode_cmd4_wifi_prov and save to FRAM
            break;

        case CMD_ID_SCHEDULE_PROV: // CMD 6
            ESP_LOGI(TAG, "[MQTT RX] Command 6 Received: Schedule Provisioning");
            // TODO: Decode payload with json_decode_cmd6_schedule and update FRAM Schedule table
            break;

        case CMD_ID_SET_IR_RAW_DATA: // CMD 8
            {
            ESP_LOGI(TAG, "[MQTT RX] Command 8 Received: Set IR Raw Data Payload");
            // TODO: Decode payload with json_decode_cmd8_ir_raw and write waveforms to FRAM
            force_sleep();
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

    // Mocked sensor readings and state
    int mocked_temp = 24;
    int mocked_humidity = 60;
    const char *mocked_rtc_time = "10:00";
    const char *mocked_rssi = "-65";
    float mocked_battery = 3.7f;
    last_action_t mocked_action = ACTION_TELEMETRY; // 0 = Telemetry

    char *json_payload = build_telemetry_json(
        mocked_temp,
        mocked_humidity,
        mocked_rtc_time,
        mocked_rssi,
        mocked_battery,
        mocked_action
    );

    if (json_payload == NULL) {
        ESP_LOGE(TAG, "Failed to build JSON payload for CMD 0");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Publishing Initial Telemetry Payload:\n%s", json_payload);
    esp_err_t err = board_mqtt_publish_uplink(json_payload, 1);

    // Free memory allocated by build_telemetry_json (cJSON)
    free(json_payload);
    if(err != -1) 
    {
        err = ESP_OK;
    }
    else
    {
        return ESP_FAIL;
    }
    return err;
}

/**
 * @brief Simulates dynamic Wi-Fi credential loading from FRAM.
 */
static esp_err_t setup_simulated_fram_wifi_credentials(void) {
    wifi_credential_t cred = {0};
    snprintf(cred.ssid, sizeof(cred.ssid), "VIVOFIBRA-56ED_EXT");
    snprintf(cred.password, sizeof(cred.password), "72233756ED");

    ESP_LOGI(TAG, "Dynamic Credential Loaded: SSID='%s'", cred.ssid);
    return board_wifi_set_dynamic_credential(&cred);
}

void app_main(void) {
    // Disable brownout detector during startup if supply fluctuates
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    // 1. Auto-Power Hold Circuit Initialization
    gpio_config_t pwr_conf = {
        .pin_bit_mask = (1ULL << ESP_REG_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&pwr_conf);

    // 2. Initialize NVS Flash (Required by ESP-IDF Wi-Fi Stack)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 3. Hardware Buses & Drivers Initialization
    ESP_ERROR_CHECK(board_i2c_bus_init());
    ESP_ERROR_CHECK(rtc_ht8563_init());
    ESP_ERROR_CHECK(fram_init());
    ESP_ERROR_CHECK(fram_ring_init());
    ESP_ERROR_CHECK(oled_init(OLED_I2C_ADDR_DEFAULT));

    ESP_LOGI(TAG, "=== explorerAirConditioner Firmware System Initialized ===");

    // 4. Create Main Queue
    s_app_event_queue = xQueueCreate(10, sizeof(app_event_t));
    if (s_app_event_queue == NULL) {
        ESP_LOGE(TAG, "Critical: Failed to create central application event queue");
        return;
    }

    // 5. Initialize Wi-Fi Driver and Register Event Handlers
    ESP_ERROR_CHECK(board_wifi_init());
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        BOARD_WIFI_EVENTS, ESP_EVENT_ANY_ID, &on_wifi_event_handler, NULL, NULL));

    // 6. Load Dynamic Wi-Fi Credentials from FRAM
    ESP_ERROR_CHECK(setup_simulated_fram_wifi_credentials());

    // 7. Analyze Boot/Wakeup Reason (Task 0)
    boot_event_t boot_cause = EVENT_BOOT_POWER_ON;

    app_event_t initial_evt = {
        .type = APP_EVENT_BOOT_ANALYZED,
        .boot_cause = boot_cause
    };
    xQueueSend(s_app_event_queue, &initial_evt, portMAX_DELAY);

    // 8. Main Application Finite State Machine (FSM Loop)
    app_event_t current_evt;
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
                    // Send Initial Telemetry (CMD 0) using mocked values
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
                    gpio_set_level(ESP_REG_GPIO, 1);
                    while (1) {
                        vTaskDelay(pdMS_TO_TICKS(1000));
                    }
                    break;

                default:
                    ESP_LOGW(TAG, "[FSM] Unhandled central application event: %d", current_evt.type);
                    break;
            }
        }
    }
}