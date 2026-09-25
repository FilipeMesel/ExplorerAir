#ifndef IR_REMOTE_H
#define IR_REMOTE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IR_RESOLUTION_HZ            1000000 /*< 1 MHz (resolution in 1 us) */
#define CARRIER_FREQ_HZ             38000   /*< 38 kHz for a default IR */
#define MAX_BUFFER_SYMBOLS          350
#define MAX_IR_BUFFER_SIZE          700     /*< Maximum raw buffer length */

/**
 * Note: This value MUST NOT be higher than 65ns. If you
 * don't respect this, the ESP32 will restart and show the log below!
 * ERROR LOG: E (282) rmt: rmt_receive(395): signal_range_max_ns too big,
 * should be less than 65535000 ns
 */
#define IR_RMT_RECEIVER_TIMEOUT     65      /**<RMT Receiver timeout callback. */

/**
 * @brief Structure with the timings (mark/space in µs) of the IR waveform
 */
typedef struct {
    uint16_t data[MAX_IR_BUFFER_SIZE];  /**< Pulse timings in microseconds */
    uint16_t length;                    /**< Total number of elements in the data array */
} ir_raw_command_t;

/**
 * @brief Initializes the RMT TX and RX channels for infrared.
 * 
 * @param gpio_tx transmit GPIO pin
 * @param gpio_rx GPIO receive pin
 * @return esp_err_t ESP_OK in case of success
 */
esp_err_t ir_remote_init(int gpio_tx, int gpio_rx);

/**
 * @brief Reads the last command received by the RMT queue (non-blocking).
 * 
 * @param[out] cmd_out Pointer to the structure where the RAW command will be saved.
 * @return esp_err_t ESP_OK if a valid command was read, ESP_ERR_NOT_FOUND if there is no data.
 */
esp_err_t ir_remote_read_last_command(ir_raw_command_t *cmd_out);

/**
 * @brief Transmits a raw IR waveform.
 * 
 * @param[in] cmd Pointer to the structure containing times in microseconds.
 * @return esp_err_t ESP_OK in the event of a successful transmission.
 */
esp_err_t ir_remote_send_command(const ir_raw_command_t *cmd);

#ifdef __cplusplus
}
#endif

#endif // IR_REMOTE_H