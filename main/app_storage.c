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

esp_err_t app_storage_save_wifi_credentials(const wifi_credentials_t *creds) {
    if (!creds) return ESP_ERR_INVALID_ARG;

    esp_err_t ret = fram_write(FRAM_ADDR_WIFI_CREDENTIALS, (const uint8_t *)creds, sizeof(wifi_credentials_t));
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Novas credenciais Wi-Fi salvas na FRAM com sucesso. SSID: %s", creds->ssid);
    } else {
        ESP_LOGE(TAG, "Falha ao gravar credenciais Wi-Fi na FRAM.");
    }
    return ret;
}

esp_err_t app_storage_get_wifi_credentials(wifi_credentials_t *creds) {
    if (!creds) return ESP_ERR_INVALID_ARG;

    esp_err_t ret = fram_read(FRAM_ADDR_WIFI_CREDENTIALS, (uint8_t *)creds, sizeof(wifi_credentials_t));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erro na leitura da FRAM para credenciais Wi-Fi.");
        return ret;
    }

    if (creds->is_valid != 1) {
        ESP_LOGW(TAG, "Nenhuma credencial de cliente válida encontrada na FRAM.");
        return ESP_ERR_NOT_FOUND;
    }

    return ESP_OK;
}

esp_err_t app_storage_save_schedule(const schedule_payload_t *schedule) {
    if (!schedule || schedule->schedule_id >= MAX_SCHEDULE_ITEMS) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t offset = FRAM_ADDR_SCHEDULE_TABLE + (schedule->schedule_id * sizeof(schedule_payload_t));
    esp_err_t ret = fram_write(offset, (const uint8_t *)schedule, sizeof(schedule_payload_t));
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Agendamento ID %d salvo na FRAM (week_days: %d, time: %s, action: %d)",
                 schedule->schedule_id, schedule->week_days, schedule->time, schedule->action);
    } else {
        ESP_LOGE(TAG, "Falha ao gravar agendamento ID %d na FRAM", schedule->schedule_id);
    }
    return ret;
}

esp_err_t app_storage_get_schedule(uint8_t schedule_id, schedule_payload_t *out_schedule) {
    if (!out_schedule || schedule_id >= MAX_SCHEDULE_ITEMS) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t offset = FRAM_ADDR_SCHEDULE_TABLE + (schedule_id * sizeof(schedule_payload_t));
    esp_err_t ret = fram_read(offset, (uint8_t *)out_schedule, sizeof(schedule_payload_t));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao ler agendamento ID %d na FRAM", schedule_id);
    }
    return ret;
}

esp_err_t app_storage_save_wakeup_context(const wakeup_context_t *ctx) {
    if (!ctx) return ESP_ERR_INVALID_ARG;
    return fram_write(FRAM_ADDR_WAKEUP_CONTEXT, (const uint8_t *)ctx, sizeof(wakeup_context_t));
}

esp_err_t app_storage_get_wakeup_context(wakeup_context_t *ctx) {
    if (!ctx) return ESP_ERR_INVALID_ARG;
    return fram_read(FRAM_ADDR_WAKEUP_CONTEXT, (uint8_t *)ctx, sizeof(wakeup_context_t));
}