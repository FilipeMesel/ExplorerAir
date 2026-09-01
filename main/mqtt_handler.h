/**
 * @file mqtt_handler.h
 * @author Filipe Mesel Lobo Costa Cardoso
 * @brief This file contains the function declarations for handling MQTT communication in the Explorer IR Blaster project.
 * @version 0.1
 * @date 2026-08-31
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <stdbool.h>

#define MAX_MQTT_PAYLOAD_LEN 2048

/**
 * @brief Structure to hold MQTT message data
 * 
 */
typedef struct {
    char payload[MAX_MQTT_PAYLOAD_LEN];
} mqtt_message_t;

/**
 * @brief Initializes the MQTT client and attempts to connect to the broker with retries.
 * @param max_retries Maximum number of connection attempts
 * @return true if connected successfully, false otherwise
 */
bool mqtt_init_with_retry(int max_retries);

/**
 * @brief Verifys if the MQTT client is currently connected to the broker.
 * @return true if connected, false otherwise
 */
bool mqtt_is_connected(void);

/**
 * @brief Publishes a JSON payload to the MQTT broker on the topic "explorer/command/<MAC_ADDRESS>".
 * @param json_payload The JSON payload to publish
 * @param retain Whether to retain the message on the broker
 * @return true if the message was published successfully, false otherwise
 */
bool mqtt_publish_status(const char *json_payload, bool retain);

/**
 * @brief Publishes a JSON payload to the MQTT broker on the topic "explorer/command/<MAC_ADDRESS>" and optionally returns the MAC address string.
 * @param json_payload The JSON payload to publish
 * @param out_mac_str Optional pointer to a buffer where the MAC address string will be copied (if not NULL)
 * @return true if the message was published successfully, false otherwise
 */
bool mqtt_publish_commands_json(const char *json_payload, char *out_mac_str);

/**
 * @brief Stops the MQTT client and disconnects from the broker.
 *
 */
void mqtt_stop(void);

#endif // MQTT_HANDLER_H