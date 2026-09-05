/**
 * @file board_mqtt.h
 * @brief MQTT Client Component with MAC Address Dynamic Topics for ESP-IDF v6.0+.
 * @author Embedded Software Engineer
 * @date 2026-09-05
 */

#ifndef BOARD_MQTT_H
#define BOARD_MQTT_H

#include "esp_err.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BOARD_MQTT_EVENT_CONNECTED,
    BOARD_MQTT_EVENT_DISCONNECTED,
    BOARD_MQTT_EVENT_DATA_RECEIVED
} board_mqtt_event_id_t;

typedef struct {
    char topic[128];
    char payload[2048];
    size_t payload_len;
} board_mqtt_data_t;

ESP_EVENT_DECLARE_BASE(BOARD_MQTT_EVENTS);

/**
 * @brief Initializes MQTT client and generates dynamic MAC topics.
 * 
 * @param broker_uri MQTT Broker URI (e.g., "mqtt://broker.hivemq.com:1883").
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t board_mqtt_init(const char *broker_uri, int buffer_size, int out_buffer_size);

/**
 * @brief Connects to MQTT broker.
 * 
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t board_mqtt_start(void);

/**
 * @brief Publishes payload to the dynamic UPLINK topic (explorerIRBlaster/{MAC}/UPLINK).
 * 
 * @param json_payload JSON String payload.
 * @param qos Quality of Service (0, 1, or 2).
 * @return int Message ID on success, -1 on failure.
 */
int board_mqtt_publish_uplink(const char *json_payload, int qos);

/**
 * @brief Stops and cleans up the MQTT Client.
 * 
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t board_mqtt_stop(void);

#ifdef __cplusplus
}
#endif

#endif // BOARD_MQTT_H