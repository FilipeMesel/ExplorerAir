#ifndef IR_REMOTE_H
#define IR_REMOTE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Definições de hardware para o driver RMT
#define IR_RESOLUTION_HZ    1000000 // 1 MHz (resolução de 1 us)
#define CARRIER_FREQ_HZ     38000   // 38 kHz para IR padrão
#define MAX_BUFFER_SYMBOLS  350     
#define MAX_IR_BUFFER_SIZE  700     // Tamanho máximo do buffer de durações RAW

/**
 * @brief Estrutura com os tempos (marca/espaço em us) da onda IR
 */
typedef struct {
    uint16_t data[MAX_IR_BUFFER_SIZE]; /**< Tempos dos pulsos em microssegundos */
    uint16_t length;                  /**< Quantidade total de elementos no array data */
} ir_raw_command_t;

/**
 * @brief Inicializa os canais de TX e RX do RMT para infravermelho.
 * 
 * @param gpio_tx pino GPIO de transmissão
 * @param gpio_rx pino GPIO de recepção
 * @return esp_err_t ESP_OK em caso de sucesso
 */
esp_err_t ir_remote_init(int gpio_tx, int gpio_rx);

/**
 * @brief Lê o último comando recebido pela fila do RMT (Não-bloqueante).
 * 
 * @param[out] cmd_out Ponteiro para a estrutura onde o comando RAW será salvo.
 * @return esp_err_t ESP_OK se leu um comando válido, ESP_ERR_NOT_FOUND se não houver dados.
 */
esp_err_t ir_remote_read_last_command(ir_raw_command_t *cmd_out);

/**
 * @brief Transmite uma forma de onda IR RAW.
 * 
 * @param[in] cmd Ponteiro para a estrutura com os tempos em us.
 * @return esp_err_t ESP_OK em caso de sucesso na transmissão.
 */
esp_err_t ir_remote_send_command(const ir_raw_command_t *cmd);

#ifdef __cplusplus
}
#endif

#endif // IR_REMOTE_H