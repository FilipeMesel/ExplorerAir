// /**
//  * @file main.c
//  * @brief Main application entry point and Event-Driven FSM for explorerAirConditioner.
//  * @author Embedded Software Architect
//  * @date 2026-09-07
//  */

// #include <stdio.h>
// #include <string.h>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "freertos/queue.h"
// #include "esp_log.h"
// #include "esp_event.h"
// #include "driver/gpio.h"
// #include "nvs_flash.h"

// #include "main.h"
// #include "fram_mb85rs512t.h"
// #include "board_i2c_bus.h"
// #include "rtc_ht8563.h"
// #include "display_oled.h"
// #include "board_wifi.h"
// #include "board_mqtt.h"
// #include "json_protocol.h"

// #define ESP_REG_GPIO 22

// static const char *TAG = "MAIN_APP";

// /**
//  * @brief System Application Events for Main Queue
//  */
// typedef enum {
//     APP_EVENT_BOOT_ANALYZED,
//     APP_EVENT_WIFI_CONNECTED,
//     APP_EVENT_WIFI_FAILOVER_EXHAUSTED,
//     APP_EVENT_TIMER_SET_SUCCESS
// } app_event_type_t;

// typedef struct {
//     app_event_type_t type;
//     boot_event_t boot_cause;
// } app_event_t;

// static QueueHandle_t s_app_event_queue = NULL;

// /**
//  * @brief Handler for Wi-Fi manager event bridge into FreeRTOS Queue.
//  */
// static void on_wifi_event_handler(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data) {
//     app_event_t evt = {0};
//     if (base == BOARD_WIFI_EVENTS) {
//         if (id == BOARD_WIFI_EVENT_CONNECTED) {
//             evt.type = APP_EVENT_WIFI_CONNECTED;
//             xQueueSend(s_app_event_queue, &evt, portMAX_DELAY);
//         } else if (id == BOARD_WIFI_EVENT_FAILOVER_EXHAUSTED) {
//             evt.type = APP_EVENT_WIFI_FAILOVER_EXHAUSTED;
//             xQueueSend(s_app_event_queue, &evt, portMAX_DELAY);
//         }
//     }
// }

// /**
//  * @brief Simulates reading/writing the dynamic Wi-Fi credentials from FRAM.
//  */
// static esp_err_t setup_simulated_fram_wifi_credentials(void) {
//     wifi_credential_t cred = {0};
//     snprintf(cred.ssid, sizeof(cred.ssid), "VIVOFIBRA-56ED_EXT");
//     snprintf(cred.password, sizeof(cred.password), "72233756ED");

//     ESP_LOGI(TAG, "Credencial Dinâmica Carregada: SSID='%s'", cred.ssid);
//     return board_wifi_set_dynamic_credential(&cred);
// }

// void app_main(void) {
//     // 1. Auto-Sustentação (Power-Hold)
//     gpio_config_t pwr_conf = {
//         .pin_bit_mask = (1ULL << ESP_REG_GPIO),
//         .mode = GPIO_MODE_OUTPUT,
//         .pull_up_en = GPIO_PULLUP_DISABLE,
//         .pull_down_en = GPIO_PULLDOWN_DISABLE,
//         .intr_type = GPIO_INTR_DISABLE
//     };
//     gpio_config(&pwr_conf);

//     // 2. Inicializa NVS (Necessário para a pilha Wi-Fi do ESP-IDF)
//     esp_err_t ret = nvs_flash_init();
//     if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
//         ESP_ERROR_CHECK(nvs_flash_erase());
//         ret = nvs_flash_init();
//     }
//     ESP_ERROR_CHECK(ret);

//     // 3. Inicialização dos Barramentos e Periféricos
//     ESP_ERROR_CHECK(board_i2c_bus_init());
//     ESP_ERROR_CHECK(rtc_ht8563_init());
//     ESP_ERROR_CHECK(fram_init());
//     ESP_ERROR_CHECK(fram_ring_init());
//     ESP_ERROR_CHECK(oled_init(OLED_I2C_ADDR_DEFAULT));

//     ESP_LOGI(TAG, "=== Sistema explorerAirConditioner Inicializado ===");

//     // 4. Inicializa Fila de Eventos da Aplicação
//     s_app_event_queue = xQueueCreate(10, sizeof(app_event_t));
//     if (s_app_event_queue == NULL) {
//         ESP_LOGE(TAG, "Falha ao criar fila de eventos principal");
//         return;
//     }

//     // 5. Configuração do Driver Wi-Fi e Handlers de Evento
//     ESP_ERROR_CHECK(board_wifi_init());
//     ESP_ERROR_CHECK(esp_event_handler_instance_register(
//         BOARD_WIFI_EVENTS, ESP_EVENT_ANY_ID, &on_wifi_event_handler, NULL, NULL));

//     // 6. Simulação de Leitura da FRAM e Configuração da Rede Dinâmica
//     ESP_ERROR_CHECK(setup_simulated_fram_wifi_credentials());

//     // 7. Análise da Causa do Boot (Tarefa 0)
//     boot_event_t boot_cause = EVENT_BOOT_POWER_ON;

//     app_event_t initial_evt = {
//         .type = APP_EVENT_BOOT_ANALYZED,
//         .boot_cause = boot_cause
//     };
//     xQueueSend(s_app_event_queue, &initial_evt, portMAX_DELAY);

//     // 8. Loop Principal (Máquina de Estados Finita Guiada por Eventos)
//     app_event_t current_evt;
//     while (1) {
//         if (xQueueReceive(s_app_event_queue, &current_evt, portMAX_DELAY) == pdTRUE) {
//             switch (current_evt.type) {

//                 case APP_EVENT_BOOT_ANALYZED:
//                     ESP_LOGI(TAG, "[FSM] Boot Processado. Iniciando Sequência de Wi-Fi Failover...");
//                     board_wifi_start_failover_connect();
//                     break;

//                 case APP_EVENT_WIFI_CONNECTED:
//                     ESP_LOGI(TAG, "[FSM] Wi-Fi Conectado com Sucesso!");

//                     // Configura a interrupção por Timer do RTC (ex: 10 segundos para teste)
//                     rtc_ht8563_clear_flags();
//                     if (rtc_ht8563_set_timer(10) == ESP_OK) {
//                         ESP_LOGI(TAG, "[RTC] Interrupção por timer de 10s configurada com sucesso!");
//                         app_event_t timer_evt = {.type = APP_EVENT_TIMER_SET_SUCCESS};
//                         xQueueSend(s_app_event_queue, &timer_evt, portMAX_DELAY);
//                     } else {
//                         ESP_LOGE(TAG, "[RTC] Falha ao configurar Timer do RTC!");
//                     }
//                     break;

//                 case APP_EVENT_WIFI_FAILOVER_EXHAUSTED:
//                     ESP_LOGE(TAG, "[FSM] ERRO: Não foi possível conectar em nenhuma rede Wi-Fi!");
//                     break;

//                 case APP_EVENT_TIMER_SET_SUCCESS:
//                     ESP_LOGI(TAG, "[FSM] Processo Concluído. Aguardando disparo da interrupção do RTC...");
//                     gpio_set_level(ESP_REG_GPIO, 1);
//                     break;

//                 default:
//                     ESP_LOGW(TAG, "[FSM] Evento não mapeado recebido: %d", current_evt.type);
//                     break;
//             }
//         }
//     }
// }

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
 * @brief Eventos Unificados da Aplicação na Fila Central
 */
typedef enum {
    APP_EVENT_BOOT_ANALYZED,
    APP_EVENT_WIFI_CONNECTED,
    APP_EVENT_WIFI_FAILOVER_EXHAUSTED,
    APP_EVENT_MQTT_CONNECTED,
    APP_EVENT_MQTT_DISCONNECTED,
    APP_EVENT_TIMER_SET_SUCCESS
} app_event_type_t;

typedef struct {
    app_event_type_t type;
    boot_event_t boot_cause;
} app_event_t;

static QueueHandle_t s_app_event_queue = NULL;

/**
 * @brief Handler de eventos do Wi-Fi repassados para a Fila Central.
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
 * @brief Handler de eventos do MQTT repassados para a Fila Central.
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
        }
    }
}

/**
 * @brief Simula/carrega as credenciais dinâmicas da FRAM.
 */
static esp_err_t setup_simulated_fram_wifi_credentials(void) {
    wifi_credential_t cred = {0};
    snprintf(cred.ssid, sizeof(cred.ssid), "SEU-WIFI");
    snprintf(cred.password, sizeof(cred.password), "123456789");

    ESP_LOGI(TAG, "Credencial Dinâmica Carregada: SSID='%s'", cred.ssid);
    return board_wifi_set_dynamic_credential(&cred);
}

void app_main(void) {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    // 1. Auto-Sustentação (Power-Hold)
    gpio_config_t pwr_conf = {
        .pin_bit_mask = (1ULL << ESP_REG_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&pwr_conf);

    // 2. Inicializa NVS (Necessário para a pilha Wi-Fi do ESP-IDF)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 3. Inicialização dos Barramentos e Periféricos
    ESP_ERROR_CHECK(board_i2c_bus_init());
    ESP_ERROR_CHECK(rtc_ht8563_init());
    ESP_ERROR_CHECK(fram_init());
    ESP_ERROR_CHECK(fram_ring_init());
    ESP_ERROR_CHECK(oled_init(OLED_I2C_ADDR_DEFAULT));

    ESP_LOGI(TAG, "=== Sistema explorerAirConditioner Inicializado ===");

    // 4. Inicializa Fila Unificada de Eventos da Aplicação
    s_app_event_queue = xQueueCreate(10, sizeof(app_event_t));
    if (s_app_event_queue == NULL) {
        ESP_LOGE(TAG, "Falha ao criar fila de eventos principal");
        return;
    }

    // 5. Configuração e Eventos do Driver Wi-Fi
    ESP_ERROR_CHECK(board_wifi_init());
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        BOARD_WIFI_EVENTS, ESP_EVENT_ANY_ID, &on_wifi_event_handler, NULL, NULL));

    // 7. Simulação de Leitura da FRAM e Configuração da Rede Dinâmica
    ESP_ERROR_CHECK(setup_simulated_fram_wifi_credentials());

    // 8. Análise da Causa do Boot
    boot_event_t boot_cause = EVENT_BOOT_POWER_ON;

    app_event_t initial_evt = {
        .type = APP_EVENT_BOOT_ANALYZED,
        .boot_cause = boot_cause
    };
    xQueueSend(s_app_event_queue, &initial_evt, portMAX_DELAY);

    // 9. Loop Principal (FSM Guiada pela Fila Central de Eventos)
    app_event_t current_evt;
    while (1) {
        if (xQueueReceive(s_app_event_queue, &current_evt, portMAX_DELAY) == pdTRUE) {
            switch (current_evt.type) {

                case APP_EVENT_BOOT_ANALYZED:
                    ESP_LOGI(TAG, "[FSM] Boot Processado. Conectando ao Wi-Fi...");
                    board_wifi_start_failover_connect();
                    break;

                case APP_EVENT_WIFI_CONNECTED:
                    ESP_LOGI(TAG, "[FSM] Wi-Fi Conectado! Inicializando MQTT no broker: %s", CONFIG_MQTT_BROKER_URI);
                    // 6. Registra Handler para os Eventos do MQTT
                    ESP_ERROR_CHECK(esp_event_handler_instance_register(
                        BOARD_MQTT_EVENTS, ESP_EVENT_ANY_ID, &on_mqtt_event_handler, NULL, NULL));
                    // Inicializa o MQTT passando as configurações mapeadas no Kconfig
                    ESP_ERROR_CHECK(board_mqtt_init(
                        CONFIG_MQTT_BROKER_URI,
                        CONFIG_MQTT_BUFFER_SIZE,
                        CONFIG_MQTT_OUT_BUFFER_SIZE
                    ));
                    ESP_ERROR_CHECK(board_mqtt_start());
                    break;

                case APP_EVENT_WIFI_FAILOVER_EXHAUSTED:
                    ESP_LOGE(TAG, "[FSM] ERRO: Falha ao conectar em todas as redes Wi-Fi!");
                    break;

                case APP_EVENT_MQTT_CONNECTED:
                    ESP_LOGI(TAG, "[FSM] Broker MQTT Conectado! Habilitando Timer de 10s no RTC HT8563...");

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
                        app_event_t timer_evt = {.type = APP_EVENT_TIMER_SET_SUCCESS};
                        xQueueSend(s_app_event_queue, &timer_evt, portMAX_DELAY);
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
                    break;

                case APP_EVENT_MQTT_DISCONNECTED:
                    ESP_LOGW(TAG, "[FSM] Desconectado do Broker MQTT.");
                    break;

                case APP_EVENT_TIMER_SET_SUCCESS:
                    ESP_LOGI(TAG, "[FSM] Trabalho concluído. Desligando percorrido RF...");
                   

                    // 4. Trava a CPU aqui para evitar continuar executando instruções enquanto desliga
                    while (1)
                    {
                        vTaskDelay(pdMS_TO_TICKS(1000));
                    }
                    break;

                default:
                    ESP_LOGW(TAG, "[FSM] Evento não reconhecido: %d", current_evt.type);
                    break;
            }
        }
    }
}