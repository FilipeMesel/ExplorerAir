#ifndef APP_IR_H
#define APP_IR_H

#include "esp_err.h"
#include "app_structs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa os periféricos RMT de TX/RX para o receptor e transmissor IR.
 * 
 * @return esp_err_t ESP_OK em caso de sucesso.
 */
esp_err_t app_ir_init(void);

/**
 * @brief Converte um 'last_action_t' para o slot de memória IR (0 a 9).
 * 
 * @param action Enum da ação (ex: ACTION_POWER_OFF, ACTION_SET_TEMP_22).
 * @param out_slot Ponteiro para receber o índice do slot (0 a 9).
 * @return esp_err_t ESP_OK se mapeado com sucesso, ESP_ERR_INVALID_ARG se a ação não for de IR.
 */
esp_err_t app_ir_action_to_slot(ir_action_slot_t action, uint8_t *out_slot);

/**
 * @brief Carrega o sinal IR da FRAM e transmite via hardware RMT.
 * 
 * @param action Ação IR que será buscada na FRAM e emitida.
 * @return esp_err_t ESP_OK em caso de sucesso na leitura e emissão.
 */
esp_err_t app_ir_dispatch_action(ir_action_slot_t action);

#ifdef __cplusplus
}
#endif

#endif // APP_IR_H