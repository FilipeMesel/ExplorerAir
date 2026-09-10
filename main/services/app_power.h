#ifndef APP_POWER_H
#define APP_POWER_H

#include "esp_err.h"
#include "app_events.h"
#include "app_structs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa os pinos de alimentação e botões de boot do gerenciador de energia.
 * 
 * @return esp_err_t ESP_OK em caso de sucesso.
 */
esp_err_t app_power_init(void);

/**
 * @brief Analisa os GPIOs e as flags do RTC para determinar a causa do boot.
 * 
 * @param[out] out_event Ponteiro onde o evento de boot detectado será armazenado.
 * @return esp_err_t ESP_OK em caso de sucesso.
 */
esp_err_t app_power_analyze_boot(boot_event_t *out_event);

/**
 * @brief Avalia agendamentos e telemetria para programar o próximo evento (TF/AF) no RTC HT8563.
 * 
 * @return esp_err_t ESP_OK em caso de sucesso.
 */
esp_err_t app_power_schedule_next_wakeup(void);

/**
 * @brief Executa o shutdown completo do sistema: agenda o próximo wakeup, para conectividades e corta o GPIO de HOLD.
 */
void app_power_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // APP_POWER_H