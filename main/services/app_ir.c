#include "app_ir.h"
#include "app_storage.h"
#include "ir_remote.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "APP_IR";

// Definição dos GPIOs configurados via Kconfig ou valores padrão
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

esp_err_t app_ir_action_to_slot(last_action_t action, uint8_t *out_slot)
{
    if (!out_slot) return ESP_ERR_INVALID_ARG;

    if (action >= ACTION_POWER_OFF && action <= ACTION_SET_TEMP_25) {
        *out_slot = (uint8_t)(action - ACTION_POWER_OFF);
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Ação %d não possui um slot IR associado.", action);
    return ESP_ERR_INVALID_ARG;
}

esp_err_t app_ir_dispatch_action(last_action_t action)
{
    uint8_t slot_idx = 0;
    esp_err_t ret = app_ir_action_to_slot(action, &slot_idx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Tentativa de disparo IR para ação inválida/não suportada: %d", action);
        return ret;
    }

    // Estrutura estática local para evitar alocar ~1.4 KB na stack da Task
    static ir_raw_command_t ir_cmd;

    // 1. Busca o comando salvo na FRAM
    ret = app_storage_get_ir_command(slot_idx, &ir_cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao buscar comando IR na FRAM para slot %d (Ação %d)", slot_idx, action);
        return ret;
    }

    // 2. Valida o tamanho do buffer de pulso
    if (ir_cmd.length == 0) {
        ESP_LOGE(TAG, "Slot IR %d está vazio! Nenhum sinal gravado.", slot_idx);
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "Disparando sinal IR (Slot %d, Pulsos: %d)...", slot_idx, ir_cmd.length);

    // 3. Emite o sinal pelo hardware RMT
    ret = ir_remote_send_command(&ir_cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erro na transmissão RMT do comando IR (err: %s)", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Sinal IR emitido com sucesso para ação %d!", action);
    return ESP_OK;
}