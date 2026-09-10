#ifndef APP_COMMS_H
#define APP_COMMS_H

#include "esp_err.h"
#include "app_events.h"
#include "app_structs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa a pilha de comunicação (Wi-Fi e MQTT) e registra callbacks de eventos.
 * @return ESP_OK em caso de sucesso.
 */
esp_err_t app_comms_init(void);

/**
 * @brief Inicia a sequência de conexão Wi-Fi com suporte a failover.
 * @return ESP_OK em caso de sucesso.
 */
esp_err_t app_comms_wifi_start_failover(void);

/**
 * @brief Envia a telemetria inicial simulada via MQTT e posta status na fila global.
 * @return ESP_OK em caso de sucesso.
 */
esp_err_t app_comms_send_initial_telemetry(void);

/**
 * @brief Processa e roteia comandos JSON recebidos via MQTT.
 * @param json_str String JSON recebida no tópico MQTT.
 * @return ESP_OK em caso de sucesso.
 */
esp_err_t app_comms_process_mqtt_command(const char *json_str);

/**
 * @brief Handler para eventos da pilha Wi-Fi. Posta eventos na g_app_event_queue.
 */
void app_comms_on_wifi_event(void *handler_args, esp_event_base_t base, int32_t id, void *data);

/**
 * @brief Handler para eventos do cliente MQTT. Posta eventos na g_app_event_queue.
 */
void app_comms_on_mqtt_event(void *handler_args, esp_event_base_t base, int32_t id, void *data);

esp_err_t app_comms_get_wifi_credentials_from_fram(void);

#ifdef __cplusplus
}
#endif

#endif // APP_COMMS_H