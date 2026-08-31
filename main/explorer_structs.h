#ifndef EXPLORER_STRUCTS_H
#define EXPLORER_STRUCTS_H
#include "driver/rmt_rx.h"

typedef struct {
    uint16_t data[200];
    size_t length;
} ir_raw_command_t;

typedef struct {
    const char *action_label;      // Usado para CMD_SYNC_LEARNED
    const ir_raw_command_t *ir_cmd;// Usado para CMD_SYNC_LEARNED
} send_cmd_params_t;

// Em explorer_structs.h
typedef struct {
    uint8_t cmd_id;             /**< ID do comando */
    char action_label[32];      /**< Nome da ação (ex: "SET_TEMP_18") */
    ir_raw_command_t ir_cmd;    /**< Comando IR raw */
} mqtt_queue_message_t;

typedef struct {
    uint8_t schedule_id; // 0 a 9
    uint8_t week_days;   // Bitmask: Bit0 = Enable, Bit1 = Dom, Bit2 = Seg, ..., Bit7 = Sab
    char time[6];        // HH:MM ex: "08:00"
    char action[32];     // ex: "SET_TEMP_18"
} schedule_t;

#endif