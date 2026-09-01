/**
 * @file explorer_structs.h
 * @author Filipe Mesel Lobo Costa Cardoso
 * @brief This file contains the structure definitions for the Explorer IR Blaster project.
 * @version 0.1
 * @date 2026-08-31
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#ifndef EXPLORER_STRUCTS_H
#define EXPLORER_STRUCTS_H
#include "driver/rmt_rx.h"

/**
 * @brief Structure to hold raw IR command data
 */
typedef struct {
    uint16_t data[200];
    size_t length;
} ir_raw_command_t;

/**
 * @brief Structure to hold parameters for sending commands
 */
typedef struct {
    const char *action_label;       /**< Label for the action (used for CMD_SYNC_LEARNED) */
    const ir_raw_command_t *ir_cmd; /**< Pointer to the IR command data */
} send_cmd_params_t;

/**
 * @brief Structure to hold messages for the MQTT queue
 */
typedef struct {
    uint8_t cmd_id;             /**< Command ID */
    char action_label[32];      /**< Name of the action (e.g., "SET_TEMP_18") */
    ir_raw_command_t ir_cmd;    /**< Raw IR command */
} mqtt_queue_message_t;

/**
 * @brief Structure to hold schedule information
 */
typedef struct {
    uint8_t schedule_id;    /**< Schedule ID */
    uint8_t week_days;      /**< Bitmask: Bit0 = Enable, Bit1 = Dom, Bit2 = Seg, ..., Bit7 = Sab */
    char time[6];           /**< Time in HH:MM format, e.g., "08:00" */
    char action[32];        /**< Action name, e.g., "SET_TEMP_18" */
} schedule_t;

#endif // EXPLORER_STRUCTS_H