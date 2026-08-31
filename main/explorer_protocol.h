/**
 * @file explorer_protocol.h
 * @author Filipe Mesel Lobo Costa Cardoso
 * @brief This file contains the function declarations for handling the Explorer protocol.
 * @version 0.1
 * @date 2026-08-31
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#ifndef EXPLORER_PROTOCOL_H
#define EXPLORER_PROTOCOL_H

/**
 * @brief Enumeration of command IDs used in the Explorer protocol.
 * 
 */
typedef enum {
    CMD_TELEMETRY        = 0,   /**< Command for telemetry */
    CMD_TELEMETRY_ACK    = 1,   /**< Command for telemetry acknowledgment */
    CMD_EXECUTE_IR       = 2,   /**< Command to execute IR command */
    CMD_EXECUTE_IR_ACK   = 3,   /**< Command for IR execution acknowledgment */
    CMD_SET_SLEEP        = 4,   /**< Command to set sleep mode */
    CMD_SET_SLEEP_ACK    = 5,   /**< Command for sleep mode configuration acknowledgment */
    CMD_SYNC_LEARNED     = 6,   /**< Command to synchronize learned commands */
    CMD_SET_WIFI         = 7,   /**< Command to configure WiFi */
    CMD_SET_WIFI_ACK     = 8,   /**< Command for WiFi configuration acknowledgment */
    CMD_SET_SCHEDULE      = 9,  /**< Command to configure scheduling */
    CMD_SET_SCHEDULE_ACK  = 10  /**< Command for scheduling configuration acknowledgment */
} command_id_t;

/**
 * @brief Gets the display name of a command based on its ID
 * @param cmd_id ID of the command
 * @return Display name of the command
 */
static const char* get_cmd_display_name(int cmd_id) {
    switch (cmd_id) {
        case CMD_TELEMETRY:         return "TELEMETRY";         /**< Command for telemetry */
        case CMD_TELEMETRY_ACK:     return "TELEMETRY ACK";     /**< Command for telemetry acknowledgment */
        case CMD_EXECUTE_IR:        return "EXECUTE IR";        /**< Command to execute IR command */
        case CMD_EXECUTE_IR_ACK:    return "IR SENT";           /**< Command for IR execution acknowledgment */
        case CMD_SET_SLEEP:         return "SET SLEEP";         /**< Command to set sleep mode */
        case CMD_SET_SLEEP_ACK:     return "SLEEP ACK";         /**< Command for sleep mode configuration acknowledgment */
        case CMD_SYNC_LEARNED:      return "SYNC LEARNED";      /**< Command to synchronize learned commands */
        case CMD_SET_WIFI:          return "SET WIFI";          /**< Command to configure WiFi */
        case CMD_SET_WIFI_ACK:      return "WIFI SET ACK";      /**< Command for WiFi configuration acknowledgment */
        case CMD_SET_SCHEDULE:      return "SET SCHEDULE";      /**< Command to configure scheduling */
        case CMD_SET_SCHEDULE_ACK:  return "SCHEDULE SET ACK";  /**< Command for scheduling configuration acknowledgment */
        default:                    return "UNKNOWN CMD";       /**< Unknown command */
    }
}

#endif // EXPLORER_PROTOCOL_H