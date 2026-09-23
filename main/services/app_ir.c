#include "app_ir.h"
#include "app_storage.h"
#include "ir_remote.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "APP_IR";

// Definition of GPIOs configured via Kconfig or default values
#ifndef CONFIG_IR_TX_GPIO
#define CONFIG_IR_TX_GPIO 25
#endif

#ifndef CONFIG_IR_RX_GPIO
#define CONFIG_IR_RX_GPIO 26
#endif

esp_err_t app_ir_init(void)
{
    ESP_LOGI(TAG, "Inicializando periférico IR RMT (TX GPIO: %d, RX GPIO: %d)...", 
             CONFIG_IR_TX_GPIO, CONFIG_IR_RX_GPIO);

    esp_err_t ret = ir_remote_init(CONFIG_IR_TX_GPIO, CONFIG_IR_RX_GPIO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar o componente ir_remote (err: %s)", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Driver IR RMT inicializado com sucesso.");
    return ESP_OK;
}

esp_err_t app_ir_action_to_slot(ir_action_slot_t action, uint8_t *out_slot)
{
    if (!out_slot) return ESP_ERR_INVALID_ARG;

    // Validates the range of valid actions.
    if (action >= IR_ACTION_POWER_OFF && action <= IR_ACTION_SET_TEMP_25) {
        *out_slot = (uint8_t)(action - 1); // 1 becomes slot 0, 2 becomes slot 1, etc.
        return ESP_OK;
    }

    ESP_LOGW("APP_IR", "Ação %d sem slot IR correspondente.", action);
    return ESP_ERR_INVALID_ARG;
}

esp_err_t app_ir_dispatch_action(ir_action_slot_t action)
{
    uint8_t slot_idx = 0;
    esp_err_t ret = app_ir_action_to_slot(action, &slot_idx);
    if (ret != ESP_OK) return ret;

    static ir_raw_command_t ir_cmd;
    ret = app_storage_get_ir_command(slot_idx, &ir_cmd);
    if (ret != ESP_OK || ir_cmd.length == 0) {
        ESP_LOGE("APP_IR", "Slot IR %d sem dados gravados.", slot_idx);
        return ESP_ERR_NOT_FOUND;
    }

    return ir_remote_send_command(&ir_cmd);
}