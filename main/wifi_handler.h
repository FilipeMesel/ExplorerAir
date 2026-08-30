#ifndef WIFI_HANDLER_H
#define WIFI_HANDLER_H

#include <stdbool.h>

// Callback com 2 parâmetros: título (linha 1) e subtítulo (linha 2)
typedef void (*wifi_status_cb_t)(const char *title, const char *subtitle);

void wifi_connect_init(wifi_status_cb_t status_cb);
bool wifi_is_connected(void);
bool wifi_is_failed(void);
void wifi_reset_connection_state(void);
bool wifi_save_nvs_credentials(const char *ssid, const char *pass);
bool is_wifi_initialized(void);

#endif // WIFI_HANDLER_H