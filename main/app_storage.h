#ifndef APP_STORAGE_H
#define APP_STORAGE_H

#include "esp_err.h"
#include "app_structs.h"

/* =========================================================================
 * MAPA DE MEMÓRIA FRAM (MB85RS512T - 64 KB Total / 0x0000 a 0xFFFF)
 * ========================================================================= */
#define FRAM_ADDR_SYS_CONFIG    0x0000 // Configurações do sistema (Intervalo de Telemetria, etc.)
#define FRAM_ADDR_RESERVED      0x0020 // Reservado para Wi-Fi/MQTT

/**
 * @brief Inicializa o módulo de armazenamento.
 * @return ESP_OK se o driver SPI e a FRAM responderem corretamente.
 */
esp_err_t app_storage_init(void);

/**
 * @brief Atualiza o intervalo de telemetria recebido no CMD 1 na FRAM.
 * @param interval_sec Novo intervalo em segundos.
 * @return ESP_OK em caso de sucesso.
 */
esp_err_t app_storage_save_telemetry_interval(uint16_t interval_sec);

/**
 * @brief Lê o intervalo de telemetria salvo na FRAM.
 * @param interval_sec Ponteiro para armazenar o valor lido.
 * @return ESP_OK em caso de sucesso.
 */
esp_err_t app_storage_get_telemetry_interval(uint16_t *interval_sec);

#endif // APP_STORAGE_H