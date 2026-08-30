#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <stdbool.h>

void wifi_connect_init(void);
bool wifi_is_connected(void);
bool wifi_is_failed(void);
void wifi_reset_connection_state(void);
bool mqtt_publish_commands_json(const char *json_payload, char *out_mac_str);
bool wifi_save_nvs_credentials(const char *ssid, const char *pass);
bool is_wifi_initialized(void);

#endif