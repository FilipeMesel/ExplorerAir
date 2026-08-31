#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <stdbool.h>

#define MAX_MQTT_PAYLOAD_LEN 2048

// Definição do tipo para a Fila do FreeRTOS
typedef struct {
    char payload[MAX_MQTT_PAYLOAD_LEN];
} mqtt_message_t;

/**
 * @brief Inicializa a conexão MQTT com suporte a retentativas.
 */
bool mqtt_init_with_retry(int max_retries);

/**
 * @brief Verifica o estado da conexão MQTT.
 */
bool mqtt_is_connected(void);

/**
 * @brief Publica dados de status com opção de retain.
 */
bool mqtt_publish_status(const char *json_payload, bool retain);

/**
 * @brief Publica o JSON de resposta dos comandos.
 */
bool mqtt_publish_commands_json(const char *json_payload, char *out_mac_str);

/**
 * @brief Interrompe a conexão MQTT.
 *
 */
void mqtt_stop(void);

#endif // MQTT_HANDLER_H