/**
 * @file ir_handler.h
 * @author Filipe Mesel Lobo Costa Cardoso
 * @brief This file contains the function declarations for handling IR communication in the Explorer IR Blaster project.
 * @version 0.1
 * @date 2026-08-31
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#ifndef IR_HANDLER_H
#define IR_HANDLER_H

#include <stdint.h>
#include <stddef.h>
#include "driver/rmt_rx.h"
#include "explorer_structs.h"

/**
 * @brief Initializes the IR hardware
 * 
 * @return void
 */
void ir_hardware_init(void);

/**
 * @brief Reads the last received IR command
 * 
 * @param cmd_out Pointer to the struct to store the IR command
 * @return true if successful, false otherwise
 */
bool ir_read_last_command(ir_raw_command_t *cmd_out);

/**
 * @brief Sends an IR command
 * 
 * @param cmd Pointer to the IR command to send
 * @return void
 */
void ir_send_command(const ir_raw_command_t *cmd);

#endif // IR_HANDLER_H