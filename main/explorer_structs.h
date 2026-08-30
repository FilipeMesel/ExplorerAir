
typedef struct {
    const char *action_label;      // Usado para CMD_SYNC_LEARNED
    const ir_raw_command_t *ir_cmd;// Usado para CMD_SYNC_LEARNED
} send_cmd_params_t;

// Em explorer_structs.h
typedef struct {
    command_id_t cmd_id;
    char action_label[32];
    ir_raw_command_t ir_cmd;
} mqtt_queue_message_t;