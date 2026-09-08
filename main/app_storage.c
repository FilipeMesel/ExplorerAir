#include "app_storage.h"
#include "fram_mb85rs512t.h" // Driver SPI bruto
#include "esp_log.h"

static const char *TAG = "APP_STORAGE";

esp_err_t app_storage_init(void) {
    esp_err_t ret = fram_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar o driver da FRAM!");
        return ret;
    }
    ESP_LOGI(TAG, "Armazenamento na FRAM inicializado com sucesso.");
    return ESP_OK;
}

esp_err_t app_storage_save_telemetry_interval(uint16_t interval_sec) {
    sys_config_t config = {0};

    // 1. Lê o bloco atual na FRAM
    esp_err_t ret = fram_read(FRAM_ADDR_SYS_CONFIG, (uint8_t *)&config, sizeof(sys_config_t));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao ler bloco de configuracao na FRAM.");
        return ret;
    }

    // 2. Atualiza apenas o campo do intervalo
    config.telemetry_interval_sec = interval_sec;

    // 3. Escreve de volta na FRAM
    ret = fram_write(FRAM_ADDR_SYS_CONFIG, (const uint8_t *)&config, sizeof(sys_config_t));
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Novo intervalo de telemetria salvo na FRAM: %u seg", interval_sec);
    } else {
        ESP_LOGE(TAG, "Erro ao escrever novo intervalo na FRAM.");
    }

    return ret;
}

esp_err_t app_storage_get_telemetry_interval(uint16_t *interval_sec) {
    if (!interval_sec) return ESP_ERR_INVALID_ARG;

    sys_config_t config;
    esp_err_t ret = fram_read(FRAM_ADDR_SYS_CONFIG, (uint8_t *)&config, sizeof(sys_config_t));
    
    if (ret == ESP_OK) {
        *interval_sec = config.telemetry_interval_sec;
    }

    return ret;
}