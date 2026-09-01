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
    CMD_TELEMETRY = 0,      /**< Command for telemetry */
    CMD_TELEMETRY_ACK,      /**< Command for telemetry acknowledgment */
    CMD_EXECUTE_IR,         /**< Command to execute IR command */
    CMD_EXECUTE_IR_ACK,     /**< Command for IR execution acknowledgment */
    CMD_SET_SLEEP,          /**< Command to set sleep mode */
    CMD_SET_SLEEP_ACK,      /**< Command for sleep mode configuration acknowledgment */
    CMD_SYNC_LEARNED,       /**< Command to synchronize learned commands */
    CMD_SET_WIFI,           /**< Command to configure WiFi */
    CMD_SET_WIFI_ACK,       /**< Command for WiFi configuration acknowledgment */
    CMD_SET_SCHEDULE,       /**< Command to configure scheduling */
    CMD_SET_SCHEDULE_ACK,   /**< Command for scheduling configuration acknowledgment */
    CMD_SET_IR_RAW,         /**< Command to set raw IR data */
    CMD_SET_IR_RAW_ACK,     /**< Command for raw IR data configuration acknowledgment */
    CMD_UNKNOWN             /**< Unknown command */
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
        case CMD_SET_IR_RAW:        return "SET IR RAW";        /**< Command to set raw IR data */
        case CMD_SET_IR_RAW_ACK:    return "IR RAW SET ACK";    /**< Command for raw IR data configuration acknowledgment */
        default:                    return "UNKNOWN CMD";       /**< Unknown command */
    }
    return "UNKNOWN CMD";                                       /**< Fallback for unknown commands */
}

#endif // EXPLORER_PROTOCOL_H