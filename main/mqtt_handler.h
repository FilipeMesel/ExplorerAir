#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <stdbool.h>

void mqtt_init(void);
bool mqtt_is_connected(void);
bool mqtt_publish_commands_json(const char *json_payload, char *out_mac_str);

#endif // MQTT_HANDLER_H