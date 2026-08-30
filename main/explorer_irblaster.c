#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "config.h"
#include "display.h"
#include "wifi_handler.h"

static const char *TAG = "EXPLORER_MAIN";

// Callback que envia as duas linhas para o display
static void on_wifi_status_change(const char *title, const char *subtitle) {
    char buffer[64];
    // O \n força a quebra para a linha de baixo no SSD1306
    snprintf(buffer, sizeof(buffer), "%s\n%s", title, subtitle);
    display_show_text(buffer);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NEW_VERSION_FOUND || ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    display_init();
    display_power(true);

    int total_attempts = 0;
    const int max_attempts = 6;

    while (total_attempts < max_attempts && !wifi_is_connected() && !wifi_is_failed()) {
        total_attempts++;
        ESP_LOGI(TAG, "Tentativa de conexão Wi-Fi global: %d/%d", total_attempts, max_attempts);
        
        wifi_connect_init(on_wifi_status_change);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (wifi_is_connected()) {
        ESP_LOGI(TAG, "Wi-Fi conectado com sucesso!");
        display_show_text("CONECTADO");
    } else {
        ESP_LOGE(TAG, "Falha permanente de Wi-Fi.");
        display_show_text("ERRO WIFI");
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}