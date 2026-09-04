#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "fram_mb85rs512t.h"

#include "board_i2c_bus.h"
#include "display_oled.h"

static const char *TAG = "MAIN_APP";

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
    // Initialize the OLED display with default I2C address
    //-----------------------------------
    ESP_ERROR_CHECK(oled_init(OLED_I2C_ADDR_DEFAULT));

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

    while (1) {
        
        ESP_LOGI(TAG, "Hello, World!");

        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second
    }

}