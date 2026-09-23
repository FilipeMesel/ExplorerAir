#ifndef APP_IR_H
#define APP_IR_H

#include "esp_err.h"
#include "app_structs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the TX/RX RMT peripherals for the IR receiver and transmitter.
 * 
 * @return esp_err_t ESP_OK in the event of success.
 */
esp_err_t app_ir_init(void);

/**
 * @brief Converts a 'last_action_t' to the IR memory slot (0 to 9).
 * 
 * @param action Action enum (e.g., ACTION_POWER_OFF, ACTION_SET_TEMP_22).
 * @param out_slot Pointer to receive the slot index (0 to 9).
 * @return esp_err_t ESP_OK if successfully mapped, ESP_ERR_INVALID_ARG if the action is not an IR action.
 */
esp_err_t app_ir_action_to_slot(ir_action_slot_t action, uint8_t *out_slot);

/**
 * @brief Loads the IR signal from FRAM and transmits it via the RMT hardware.
 * 
 * @param action IR action to be sought from FRAM and issued.
 * @return esp_err_t ESP_OK in the event of successful reading and issuance.
 */
esp_err_t app_ir_dispatch_action(ir_action_slot_t action);

#ifdef __cplusplus
}
#endif

#endif // APP_IR_H