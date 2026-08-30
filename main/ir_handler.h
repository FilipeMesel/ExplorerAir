#ifndef IR_HANDLER_H
#define IR_HANDLER_H

#include <stdint.h>
#include <stddef.h>
#include "driver/rmt_rx.h"

typedef struct {
    uint16_t data[200];
    size_t length;
} ir_raw_command_t;

void ir_hardware_init(void);
bool ir_read_last_command(ir_raw_command_t *cmd_out);
void ir_send_command(const ir_raw_command_t *cmd);

#endif