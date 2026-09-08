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

#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"

#define GPIO_POWER_HOLD_PIN   GPIO_NUM_22
#define GPIO_BTN_MENU_SELECT  GPIO_NUM_5
#define GPIO_BTN_MENU_ENTER   GPIO_NUM_38

#define BUTTON_HOLD_DURATION_MS 2000
#define BUTTON_POLL_INTERVAL_MS 50

static esp_err_t init_boot_gpios(void);
static bool check_dual_button_hold(void);

static const char *TAG = "MAIN_APP";

// 3. Declarar a fila global
static QueueHandle_t s_app_event_queue = NULL;

static void force_sleep(void)
{
    // 1. Limpa flags residuais do RTC
    rtc_ht8563_clear_flags();

    // 2. Ajusta hora do RTC
    rtc_date_time_t dt_initial = {
        .second = 50,
        .minute = 1,
        .hour = 0,
        .day = 1,
        .weekday = 1,
        .month = 1,
        .year = 2026
    };

    if (rtc_ht8563_set_time(&dt_initial) == ESP_OK) {
        ESP_LOGI(TAG, "Hora inicial ajustada para: 00:01:50");
    } else {
        ESP_LOGE(TAG, "Falha ao definir hora inicial no RTC");
    }

    // 3. Configura alarme do RTC
    if (rtc_ht8563_set_alarm(0, 2) == ESP_OK) {
        ESP_LOGI(TAG, "Alarme programado com sucesso para 00:02:00");
    } else {
        ESP_LOGE(TAG, "Falha ao configurar alarme no RTC");
    }

    // 4. Parada dos periféricos de rede
    board_mqtt_stop();
    board_wifi_stop();
    vTaskDelay(pdMS_TO_TICKS(100));

    // 5. Corta a energia acionando o pino do LDO/Regulador
    ESP_LOGI(TAG, "Desligando alimentação via GPIO_POWER_HOLD_PIN...");
    gpio_set_level(GPIO_POWER_HOLD_PIN, 1);
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
            {
                cmd1_sync_data_t sync_data;
                if (json_decode_sync(payload, &sync_data) == ESP_OK)
                {
                    // 1. Atualizar o RTC HT8563 com os dados recebidos
                    rtc_date_time_t dt = {
                        .hour = (uint8_t)sync_data.sync_time_t.hour,
                        .minute = (uint8_t)sync_data.sync_time_t.minute,
                        .second = (uint8_t)sync_data.sync_time_t.second,
                        .weekday = (uint8_t)sync_data.sync_time_t.weekday,
                        .day = (uint8_t)sync_data.sync_time_t.day, // Manter valores padrão se não fornecidos no CMD 1
                        .month = (uint8_t)sync_data.sync_time_t.month,
                        .year = (uint16_t)sync_data.sync_time_t.year};

                    if (rtc_ht8563_set_time(&dt) == ESP_OK)
                    {
                        ESP_LOGI(TAG, "RTC atualizado com sucesso via CMD 1: %02d:%02d:%02d",
                                 dt.hour, dt.minute, dt.second);
                    }
                    else
                    {
                        ESP_LOGE(TAG, "Falha ao atualizar hora no RTC");
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
                    ESP_LOGE(TAG, "Erro ao decodificar JSON do CMD 1");
                }
            }
            break;

        case CMD_ID_GET_IR_LEARNED: // CMD 2
            ESP_LOGI(TAG, "[MQTT RX] Command 2 Received: Get IR Learned Queue Request");
            // TODO: Process request to send learned IR pulse data to platform
            break;

        case CMD_ID_WIFI_PROV: // CMD 4
            ESP_LOGI(TAG, "[MQTT RX] Command 4 Received: Wi-Fi Credentials Provisioning");
            // TODO: Decode payload and save to FRAM
            break;

        case CMD_ID_SCHEDULE_PROV: // CMD 6
            ESP_LOGI(TAG, "[MQTT RX] Command 6 Received: Schedule Provisioning");
            // TODO: Decode payload with and update FRAM Schedule table
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
        .temp = 24,                      // 24 °C
        .umid = 58,                      // 58% de umidade
        .hour = 14,                      // 14h
        .minute = 30,                    // 30m
        .rssi = -65,                     // -65 dBm
        .battery_mv = 3700,              // 3.7 V (3700 mV)
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
    wifi_credential_t cred = {0};
    snprintf(cred.ssid, sizeof(cred.ssid), "VIVOFIBRA-56ED_EXT");
    snprintf(cred.password, sizeof(cred.password), "72233756ED");

    ESP_LOGI(TAG, "Dynamic Credential Loaded: SSID='%s'", cred.ssid);
    return board_wifi_set_dynamic_credential(&cred);
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

        case EVENT_BOOT_POWER_ON:
            ESP_LOGI(TAG, "[BOOT CAUSE] Hard Reset / Power-On Reset detected.");
            // TODO: Initialize system defaults, perform RTC sanity check, and check FRAM state.
            break;

        case EVENT_WAKEUP_BUTTON_DUAL_HOLD:
            ESP_LOGI(TAG, "[BOOT CAUSE] Dual Button Hold (>= 2s) confirmed. Triggering IR Learn/Test Mode.");
            // TODO: Initialize OLED display menu with "Aprender" and "Testar" options.
            break;

        case EVENT_WAKEUP_SINGLE_BUTTON:
            ESP_LOGI(TAG, "[BOOT CAUSE] GPIO 38 Pressed. Single-Button Wakeup.");
            // TODO: Turn on OLED screen briefly, show battery status / quick telemetry, and reset sleep timer.
            break;

        case EVENT_WAKEUP_RTC_TIMER:
            ESP_LOGI(TAG, "[BOOT CAUSE] RTC Periodic Timer (TF) Triggered.");
            // TODO: Execute Task 1 (Read action from FRAM), Task 2 (Send MQTT telemetry), and update next wakeup (Task 3).
            break;

        case EVENT_WAKEUP_RTC_ALARM:
            ESP_LOGI(TAG, "[BOOT CAUSE] RTC Schedule Alarm (AF) Triggered.");
            // TODO: Read pending schedule from FRAM, send IR command to Air Conditioner, and send execution log via MQTT.
            break;

        case EVENT_WAKEUP_SCHEDULE_TELEMETRY_CONFLICT:
            ESP_LOGW(TAG, "[BOOT CAUSE] Conflict: RTC Timer and Schedule Alarm triggered simultaneously!");
            // TODO: Execute scheduled IR action first, then immediately package and publish telemetry log via MQTT.
            break;

        case EVENT_LOW_BATTERY_SHUTDOWN:
            ESP_LOGE(TAG, "[BOOT CAUSE] Battery critically low! Preparing emergency shutdown.");
            // TODO: Save state, disable Wi-Fi/MQTT, show "Bateria Fraca" on OLED, and force sleep.
            break;

        default:
            ESP_LOGW(TAG, "[BOOT CAUSE] Unknown Wakeup Event: %d", *out_event);
            // TODO: Fallback handling, log warning to FRAM, and power down safely.
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