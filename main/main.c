#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "app_events.h"
#include "app_structs.h"
#include "app_storage.h"
#include "services/app_comms.h"
#include "services/app_power.h"

#include "board_i2c_bus.h"
#include "rtc_ht8563.h"
#include "display_oled.h"
#include "board_wifi.h"

#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"

static const char *TAG = "MAIN_APP";

QueueHandle_t g_app_event_queue = NULL;

static esp_err_t board_hardware_init(void) {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    // Inicializa gerenciador de energia (HOLD pin + BTNs)
    esp_err_t ret = app_power_init();
    if (ret != ESP_OK) return ret;

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) return ret;

    ret = board_i2c_bus_init();
    if (ret != ESP_OK) return ret;

    ret = rtc_ht8563_init();
    if (ret != ESP_OK) return ret;

    ret = app_storage_init();
    if (ret != ESP_OK) return ret;

    ret = oled_init(OLED_I2C_ADDR_DEFAULT);
    if (ret != ESP_OK) return ret;

    ret = board_wifi_init();
    if (ret != ESP_OK) return ret;

    ret = app_comms_init();
    if (ret != ESP_OK) return ret;

    return ESP_OK;
}

// Subtarefa 5.3: Refatoração da FSM principal para consumo de eventos desacoplados
static void app_fsm_task(void *pvParameters) {
   app_event_t current_evt;
    boot_event_t boot_cause = EVENT_BOOT_POWER_ON;
    
    // Tenta analisar o boot; se falhar, assume Power-On normal
    if (app_power_analyze_boot(&boot_cause) != ESP_OK) {
        ESP_LOGW(TAG, "Falha ao analisar o boot cause. Assumindo Boot padrão.");
    }

    app_event_t initial_evt = {
        .type = APP_EVENT_BOOT_ANALYZED,
        .boot_cause = boot_cause
    };
    xQueueSend(g_app_event_queue, &initial_evt, portMAX_DELAY);

    while (1) {
        if (xQueueReceive(g_app_event_queue, &current_evt, portMAX_DELAY) == pdTRUE) {
            switch (current_evt.type) {

                case APP_EVENT_BOOT_ANALYZED:
                    ESP_LOGI(TAG, "[FSM] Evento de Boot Processado (Causa: %d).", current_evt.boot_cause);

                    if (current_evt.boot_cause == EVENT_WAKEUP_BUTTON_DUAL_HOLD)
                    {
                        ESP_LOGI(TAG, "Modo de Configuração Local / IR Learn ativo.");
                        // Não inicia Wi-Fi se for apenas modo local
                    }
                    else
                    {
                        ESP_LOGI(TAG, "Iniciando Wi-Fi failover...");
                        app_comms_wifi_start_failover();
                    }
                    break;

                case APP_EVENT_WIFI_CONNECTED:
                    ESP_LOGI(TAG, "[FSM] Wi-Fi Conectado. Conectando ao Broker MQTT...");
                    board_mqtt_start();
                    break;

                case APP_EVENT_WIFI_FAILOVER_EXHAUSTED:
                    ESP_LOGW(TAG, "[FSM] Falha no Wi-Fi. Solicitando shutdown do sistema...");
                    app_power_shutdown();
                    break;

                case APP_EVENT_MQTT_CONNECTED:
                    ESP_LOGI(TAG, "[FSM] MQTT Conectado. Enviando telemetria inicial...");
                    app_comms_send_initial_telemetry();
                    break;

                case APP_EVENT_MQTT_DATA_RECEIVED:
                    ESP_LOGI(TAG, "[FSM] Dados MQTT recebidos no tópico: %s", current_evt.mqtt_data.topic);
                    app_comms_process_mqtt_command(current_evt.mqtt_data.payload);
                    break;

                case APP_EVENT_TIMER_SET_SUCCESS:
                case APP_EVENT_SHUTDOWN_REQUESTED:
                    ESP_LOGI(TAG, "[FSM] Solicitação de shutdown. Executando rotina de desligamento...");
                    app_power_shutdown();
                    break;

                default:
                    ESP_LOGD(TAG, "[FSM] Evento não tratado: %d", current_evt.type);
                    break;
            }
        }
    }
}

void app_main(void) {
    // Subtarefa 5.2: Alocação da fila de eventos principal no app_main()
    g_app_event_queue = xQueueCreate(10, sizeof(app_event_t));
    if (g_app_event_queue == NULL) {
        ESP_LOGE(TAG, "Falha ao criar a fila global de eventos!");
        return;
    }

    ESP_ERROR_CHECK(board_hardware_init());

    xTaskCreate(app_fsm_task, "app_fsm_task", 8192, NULL, 5, NULL);
}