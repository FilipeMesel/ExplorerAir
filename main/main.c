#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_timer.h"

#include "board_i2c_bus.h"
#include "rtc_ht8563.h"
#include "board_wifi.h"

#include "app_events.h"
#include "app_structs.h"
#include "app_storage.h"
#include "services/app_comms.h"
#include "services/app_power.h"
#include "services/app_ui.h"
#include "services/app_buttons.h"
#include "services/app_ir.h"

#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"

#define MQTT_CONNECTED_TIMEOUT      10000000ULL

static const char *TAG = "MAIN_APP";

QueueHandle_t g_app_event_queue = NULL;

static esp_timer_handle_t g_shutdown_timer = NULL;

void app_main_refresh_shutdown_timer(void) {
    if (g_shutdown_timer != NULL) {
        esp_timer_stop(g_shutdown_timer);
        esp_timer_start_once(g_shutdown_timer, MQTT_CONNECTED_TIMEOUT);
        ESP_LOGI(TAG, "[TIMER] Timer de shutdown reiniciado (30s).");
    }
}

void app_main_stop_shutdown_timer(void) {
    if (g_shutdown_timer != NULL) {
        esp_timer_stop(g_shutdown_timer);
        ESP_LOGI(TAG, "[TIMER] Timer de shutdown pausado.");
    }
}

static void shutdown_timer_callback(void* arg) {
    ESP_LOGI(TAG, "[TIMER 30s] Tempo limite atingido! Solicitando shutdown...");
    app_event_t evt = {
        .type = APP_EVENT_SHUTDOWN_REQUESTED
    };
    if (g_app_event_queue) {
        xQueueSend(g_app_event_queue, &evt, 0);
    }
}

static void init_shutdown_timer(void) {
    const esp_timer_create_args_t timer_args = {
        .callback = &shutdown_timer_callback,
        .name = "shutdown_30s_timer"
    };
    esp_timer_create(&timer_args, &g_shutdown_timer);
}

static esp_err_t board_hardware_init(void) {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    // Initializes power manager (HOLD pin + BTNs)
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

    ret = app_ir_init();
    if (ret != ESP_OK) return ret;

    ret = oled_init(OLED_I2C_ADDR_DEFAULT);
    if (ret != ESP_OK) return ret;

    ret = board_wifi_init();
    if (ret != ESP_OK) return ret;

    ret = app_comms_init();
    if (ret != ESP_OK) return ret;

    return ESP_OK;
}

static void ir_sequence_task(void *pvParameters) {
    app_comms_run_ir_sequence();
    vTaskDelete(NULL);
}

static void app_fsm_task(void *pvParameters) {
   app_event_t current_evt;
    boot_event_t boot_cause = EVENT_BOOT_POWER_ON;
    
    // Attempts to analyze the boot process; if it fails, it assumes a normal power-on.
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

                case APP_EVENT_BOOT_ANALYZED: {
                    ESP_LOGI(TAG, "[FSM] Evento de Boot Processado (Causa: %d).", current_evt.boot_cause);

                    if (current_evt.boot_cause == EVENT_WAKEUP_BUTTON_DUAL_HOLD)
                    {
                        ESP_LOGI(TAG, "Modo de Configuração Local / IR Learn ativo.");
                        app_buttons_start_menu_task();
                    }
                    else
                    {
                        wakeup_context_t wakeup_ctx = {0};
                        if (app_storage_get_wakeup_context(&wakeup_ctx) == ESP_OK)
                        {
                            last_action_t bm = (last_action_t)wakeup_ctx.pending_action;

                            // Decodifica Bit 0 (Wakeup Reason)
                            if (GET_LAST_ACTION_REASON(bm) == WAKEUP_REASON_SCHEDULE)
                            {
                                // Extrai Bits 1..4 (Slot da Ação)
                                ir_action_slot_t slot_to_exec = (ir_action_slot_t)GET_LAST_ACTION_ACTION(bm);

                                if (slot_to_exec != IR_ACTION_NONE)
                                {
                                    ESP_LOGI(TAG, "[FSM] Boot via Agendamento! Disparando Ação Slot: %d", slot_to_exec);
                                    app_ir_dispatch_action(slot_to_exec);
                                }
                            }
                        }

                        app_comms_wifi_start_failover();
                    }
                    break;
                }

                case APP_EVENT_EXIT_MENU_TRIGGER_TELEMETRY:
                    ESP_LOGI(TAG, "[FSM] Saida do Modo Local solicitada. Iniciando conexao para telemetria...");
                    app_ui_post_message("CONECTANDO...", "ENVIANDO DADOS", 0);
                    app_comms_wifi_start_failover();
                    break;

                case APP_EVENT_WIFI_CONNECTED:
                    ESP_LOGI(TAG, "[FSM] Wi-Fi Conectado. Conectando ao Broker MQTT...");
                    board_mqtt_start();
                    break;

                case APP_EVENT_WIFI_FAILOVER_EXHAUSTED:
                {
                    ESP_LOGW(TAG, "[FSM] Falha no Wi-Fi. Solicitando shutdown do sistema...");

                    // Dynamic reading of the wakeup context saved in FRAM
                    wakeup_context_t wakeup_ctx = {0};
                    last_action_t action = IR_ACTION_NONE;

                    if (app_storage_get_wakeup_context(&wakeup_ctx) == ESP_OK)
                    {
                        action = wakeup_ctx.pending_action;
                    }

                    // Captures the current reading to save to memory.
                    telemetry_data_t offline_telemetry = {
                        .temp = 24,
                        .umid = 58,
                        .rssi = 0, // Sem Wi-Fi
                        .battery_mv = 3700,
                        .last_action = action
                    };
                    rtc_ht8563_get_time(&offline_telemetry.sync_time_t);

                    // Save to the FRAM FIFO queue.
                    app_storage_push_telemetry_log(&offline_telemetry);

                    app_ui_post_wifi_error();
                    vTaskDelay(pdMS_TO_TICKS(3000));
                    app_ui_post_clear();
                    app_power_shutdown();
                }
                    break;

                case APP_EVENT_MQTT_CONNECTED:
                {
                    ESP_LOGI(TAG, "[FSM] MQTT Conectado. Processando estado do dispositivo...");
                    app_ui_post_message("WIFI", "CONECTADO", 100);

                    // Envia a telemetria inicial (CMD 0)
                    app_comms_send_initial_telemetry();

                    wakeup_context_t ctx = {0};
                    if (app_storage_get_wakeup_context(&ctx) == ESP_OK)
                    {
                        uint8_t is_download = GET_LAST_ACTION_DOWNLOAD(ctx.pending_action);
                        ir_action_slot_t action = (ir_action_slot_t)GET_LAST_ACTION_ACTION(ctx.pending_action);

                        if (is_download)
                        {
                            ESP_LOGI(TAG, "[FSM] Solicitando início da sequência de Download (CMD 9 idx=255)...");
                            app_comms_send_ir_download_ack();

                        }
                        else if (action == IR_ACTION_POWER_OFF)
                        {
                            ESP_LOGI(TAG, "[FSM] Disparando CMD 3 (IR Power Off)...");
                            xTaskCreate(ir_sequence_task, "ir_seq_task", 8192, NULL, 5, NULL);
                        }

                        if (g_shutdown_timer != NULL)
                        {
                            esp_timer_start_once(g_shutdown_timer, MQTT_CONNECTED_TIMEOUT);
                            ESP_LOGI(TAG, "[FSM] Timer de shutdown iniciado.");
                        }
                    }
                    break;
                }

                    case APP_EVENT_MQTT_DATA_RECEIVED:
                        ESP_LOGI(TAG, "[FSM] Dados MQTT recebidos no tópico: %s", current_evt.mqtt_data.topic);
                        if (current_evt.mqtt_data.payload != NULL)
                        {

                            app_comms_process_mqtt_command(current_evt.mqtt_data.payload);

                            free(current_evt.mqtt_data.payload);
                            current_evt.mqtt_data.payload = NULL;
                        }
                        break;

                    case APP_EVENT_MQTT_DISCONNECTED:
                    {
                        // Dynamic reading of the wakeup context saved in FRAM
                        wakeup_context_t wakeup_ctx = {0};
                        last_action_t action = IR_ACTION_NONE;

                        if (app_storage_get_wakeup_context(&wakeup_ctx) == ESP_OK)
                        {
                            action = wakeup_ctx.pending_action;
                        }

                        // Captures the current reading to save to memory.
                        telemetry_data_t offline_telemetry = {
                            .temp = 24,
                            .umid = 58,
                            .rssi = 0, // No Wi-Fi
                            .battery_mv = 3700,
                            .last_action = action};
                        rtc_ht8563_get_time(&offline_telemetry.sync_time_t);

                        // Save to the FRAM FIFO queue.
                        app_storage_push_telemetry_log(&offline_telemetry);

                        app_ui_post_clear();
                        ESP_LOGI(TAG, "[FSM] Solicitação de shutdown. Executando rotina de desligamento...");
                        app_power_shutdown();
                    }
                    break;

                case APP_EVENT_IR_TRANSFER_FAILED:
                    ESP_LOGE(TAG, "[FSM] Falha crítica na transmissão IR (CMD 3 / CMD 2). Exibindo mensagem e desligando...");

                    // Exibe no OLED a mensagem solicitada
                    app_ui_post_message("ERRO ENVIO IR", "DESLIGANDO...", 0);
                    vTaskDelay(pdMS_TO_TICKS(3000));

                    app_ui_post_clear();
                    app_power_shutdown();
                    break;

                case APP_EVENT_TIMER_SET_SUCCESS:
                case APP_EVENT_SHUTDOWN_REQUESTED:
                    app_ui_post_clear();
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

    // Allocation of the main event queue in app_main()
    g_app_event_queue = xQueueCreate(APP_MAIN_EVT_QUEUE, sizeof(app_event_t));
    if (g_app_event_queue == NULL) {
        ESP_LOGE(TAG, "Falha ao criar a fila global de eventos!");
        return;
    }

    ESP_ERROR_CHECK(board_hardware_init());

    app_ui_init();
    app_ui_post_header(4200, "FW 1.0");
    app_ui_post_booting();

    init_shutdown_timer();

    xTaskCreate(app_fsm_task, "app_fsm_task", 12288, NULL, 5, NULL);
}