#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "sdkconfig.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "fram_mb85rs512t.h"

#include "board_i2c_bus.h"
#include "display_oled.h"
#include "rtc_ht8563.h"

#include "board_wifi.h"
#include "board_mqtt.h"

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
        if (event_id == BOARD_MQTT_EVENT_CONNECTED) {
            ESP_LOGI(TAG, "MQTT OK. Enviando Telemetria (CMD 0)...");
            board_mqtt_publish_uplink("{\"cmd_id\":0,\"temp\":24,\"umid\":60,\"rtc\":\"10:00\",\"RSSI\":\"-10\",\"bat\":3.7,\"last_action\":0}", 1);
        } else if (event_id == BOARD_MQTT_EVENT_DATA_RECEIVED) {
            board_mqtt_data_t *msg = (board_mqtt_data_t *)event_data;
            ESP_LOGI(TAG, "Comando MQTT recebido: %s", msg->payload);
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
    wifi_credential_t dynamic_cred = {.ssid = "SEUWIFI", .password = "123456789"};
    board_wifi_set_dynamic_credential(&dynamic_cred);

    board_wifi_start_failover_connect();

#if CONFIG_ENABLE_FRAM_TESTS
    ESP_LOGI(TAG, "=================================================");
    ESP_LOGI(TAG, "      EXECUTANDO TESTES DA MEMÓRIA FRAM          ");
    ESP_LOGI(TAG, "=================================================");

    // Test 1: Reading the Status Register (RDSR)
    uint8_t status_reg = 0;
    if (fram_read_status(&status_reg) == ESP_OK) {
        ESP_LOGI(TAG, "[TESTE 1] Status Register obtido: 0x%02X", status_reg);
    } else {
        ESP_LOGE(TAG, "[TESTE 1] Falha ao ler Status Register");
    }

    // Test 2: Writing and Reading at the Configuration Offset
    const char write_config[] = "CONFIG_IR_BLASTER_V1";
    char read_config[30] = {0};

    ESP_LOGI(TAG, "[TESTE 2] Escrevendo na Seção de Configuração...");
    fram_write_config((uint8_t *)write_config, strlen(write_config));

    fram_read_config((uint8_t *)read_config, strlen(write_config));
    ESP_LOGI(TAG, "[TESTE 2] Lido: %s", read_config);

    if (strcmp(write_config, read_config) == 0) {
        ESP_LOGI(TAG, "[TESTE 2] PASSOU: Leitura/Escrita de Configuração OK!");
    } else {
        ESP_LOGE(TAG, "[TESTE 2] FALHOU: Dados divergentes!");
    }

    // Test 3: Testing the Telemetry Ring Buffer Section
    uint8_t telemetry_data[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint8_t telemetry_read[4] = {0};

    ESP_LOGI(TAG, "[TESTE 3] Escrevendo Telemetria no Ring Buffer (Offset relativo 0x00)...");
    fram_write_telemetry_ring(0x00, telemetry_data, sizeof(telemetry_data));

    fram_read_telemetry_ring(0x00, telemetry_read, sizeof(telemetry_read));

    if (memcmp(telemetry_data, telemetry_read, sizeof(telemetry_data)) == 0) {
        ESP_LOGI(TAG, "[TESTE 3] PASSOU: Ring Buffer de Telemetria OK!");
    } else {
        ESP_LOGE(TAG, "[TESTE 3] FALHOU: Falha na validação da Telemetria!");
    }

    ESP_LOGI(TAG, "=================================================");
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