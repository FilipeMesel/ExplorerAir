#ifndef APP_STORAGE_H
#define APP_STORAGE_H

#include "esp_err.h"
#include "app_structs.h"

/* =========================================================================
 * MAPA DE MEMÓRIA FRAM (MB85RS512T - 64 KB Total / 0x0000 a 0xFFFF)
 * ========================================================================= */
#define FRAM_ADDR_SYS_CONFIG        0x0000 /**< Configurações do sistema (Intervalo de Telemetria, etc.) */
#define FRAM_ADDR_WIFI_CREDENTIALS  0x0020 /**< Localização das credenciais Wi-Fi do cliente (97 bytes) */
#define FRAM_ADDR_SCHEDULE_TABLE    0x0100 /**< Tabela de agendamentos (11 * sizeof(schedule_payload_t)) */
#define FRAM_ADDR_WAKEUP_CONTEXT    0x0200 /**< Contexto do próximo wakeup */
#define FRAM_ADDR_RING_BUFFER_LOGS  0x0300 /**< Fila FIFO / Ring Buffer de Telemetrias Offline */

/**
 * @brief Inicializa o módulo de armazenamento e o driver da FRAM.
 * @return ESP_OK se o driver SPI e a FRAM responderem corretamente.
 */
esp_err_t app_storage_init(void);

/**
 * @brief Atualiza o intervalo de telemetria na FRAM.
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

/**
 * @brief Salva as credenciais de Wi-Fi dinâmicas na FRAM.
 * @param creds Ponteiro para a estrutura com SSID e Senha.
 * @return ESP_OK em caso de sucesso.
 */
esp_err_t app_storage_save_wifi_credentials(const wifi_credentials_t *creds);

/**
 * @brief Lê as credenciais de Wi-Fi dinâmicas salvas na FRAM.
 * @param creds Ponteiro onde as credenciais serão carregadas.
 * @return ESP_OK se carregado e válido, ESP_ERR_NOT_FOUND se não houver credencial salva.
 */
esp_err_t app_storage_get_wifi_credentials(wifi_credentials_t *creds);

/**
 * @brief Salva ou atualiza um agendamento específico no índice schedule_id na FRAM.
 * @param schedule Ponteiro para a estrutura com os dados do agendamento.
 * @return ESP_OK em caso de sucesso.
 */
esp_err_t app_storage_save_schedule(const schedule_payload_t *schedule);

/**
 * @brief Lê um agendamento específico armazenado na FRAM pelo seu ID (0 a 10).
 * @param schedule_id ID do agendamento a ser lido.
 * @param out_schedule Ponteiro onde os dados serão armazenados.
 * @return ESP_OK em caso de sucesso.
 */
esp_err_t app_storage_get_schedule(uint8_t schedule_id, schedule_payload_t *out_schedule);

/**
 * @brief Persiste o contexto do próximo evento de despertar na FRAM.
 * @param ctx Ponteiro para a estrutura de contexto do wakeup.
 * @return ESP_OK em caso de sucesso.
 */
esp_err_t app_storage_save_wakeup_context(const wakeup_context_t *ctx);

/**
 * @brief Recupera o contexto do wakeup armazenado na FRAM.
 * @param ctx Ponteiro para preenchimento dos dados.
 * @return ESP_OK em caso de sucesso.
 */
esp_err_t app_storage_get_wakeup_context(wakeup_context_t *ctx);

/**
 * @brief Insere uma estrutura telemetry_data_t na Fila FIFO na FRAM.
 *        Se atingir 100 registros, o registro mais antigo é sobrescrito.
 */
esp_err_t app_storage_push_telemetry_log(const telemetry_data_t *log_entry);

/**
 * @brief Remove e retorna a telemetry_data_t mais antiga da Fila FIFO.
 */
esp_err_t app_storage_pop_telemetry_log(telemetry_data_t *out_entry);

/**
 * @brief Obtém a quantidade de registros pendentes na FRAM.
 */
esp_err_t app_storage_get_telemetry_log_count(uint16_t *out_count);

/**
 * @brief Reseta a fila FIFO na FRAM.
 */
esp_err_t app_storage_clear_telemetry_queue(void);

#endif // APP_STORAGE_H