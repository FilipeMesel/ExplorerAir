/**
 * @file mqtt_handler.c
 * @author Filipe Mesel Lobo Costa Cardoso
 * @brief This file contains the implementation of MQTT communication handling for the Explorer IR Blaster project.
 * @version 0.1
 * @date 2026-08-31
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include "mqtt_handler.h"
#include "wifi_handler.h"
#include "config.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "MQTT_HANDLER";

#define MQTT_CONNECTED_BIT BIT0

static EventGroupHandle_t s_mqtt_event_group = NULL;    /**< Event group for MQTT connection status */
static esp_mqtt_client_handle_t mqtt_client = NULL;     /**< MQTT client handle */
static char s_mac_str[13] = {0};                        /**< MAC address string */

extern QueueHandle_t g_mqtt_queue;                      /**< Global MQTT message queue */

bool mqtt_is_connected(void) {
    if (s_mqtt_event_group == NULL) return false;
    EventBits_t bits = xEventGroupGetBits(s_mqtt_event_group);
    return (bits & MQTT_CONNECTED_BIT) != 0;
}

void mqtt_stop(void) {
    if (mqtt_client != NULL) {
        esp_mqtt_client_stop(mqtt_client);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Conectado ao Broker MQTT!");
            if (s_mqtt_event_group) {
                xEventGroupSetBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
            }
            
            // Subscribe to the topic "explorer/device/<MAC_ADDRESS>/set" for receiving commands
            char sub_topic[64];
            snprintf(sub_topic, sizeof(sub_topic), "explorer/device/%s/set", s_mac_str);
            esp_mqtt_client_subscribe(mqtt_client, sub_topic, 1);
            ESP_LOGI(TAG, "Inscrito no tópico: %s", sub_topic);
            break;

        case MQTT_EVENT_DATA:
            if (event->data_len > 0 && g_mqtt_queue != NULL) {
                mqtt_message_t msg;
                size_t len = event->data_len < (MAX_MQTT_PAYLOAD_LEN - 1) ? event->data_len : (MAX_MQTT_PAYLOAD_LEN - 1);
                memcpy(msg.payload, event->data, len);
                msg.payload[len] = '\0';

                // Send a message to the MQTT queue for processing in another task
                if (xQueueSend(g_mqtt_queue, &msg, pdMS_TO_TICKS(100)) != pdTRUE) {
                    ESP_LOGE(TAG, "Fila de mensagens MQTT cheia! Descartando pacote.");
                }
            }
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Desconectado do Broker MQTT.");
            if (s_mqtt_event_group) {
                xEventGroupClearBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
            }
            break;

        default:
            break;
    }
}

bool mqtt_init_with_retry(int max_retries) {
    if (!wifi_is_connected()) {
        ESP_LOGE(TAG, "Impossível inicializar MQTT sem Wi-Fi!");
        return false;
    }

    if (s_mqtt_event_group == NULL) {
        s_mqtt_event_group = xEventGroupCreate();
    }

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    sprintf(s_mac_str, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    if (mqtt_client == NULL) {
        esp_mqtt_client_config_t mqtt_cfg = {
            .broker.address.uri = MQTT_BROKER_URI
        };
        mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
        esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    }

    for (int attempt = 1; attempt <= max_retries; attempt++) {
        ESP_LOGI(TAG, "Tentativa de conexão MQTT (%d/%d)...", attempt, max_retries);
        xEventGroupClearBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
        esp_mqtt_client_start(mqtt_client);

        // Wait for the MQTT connection event with a timeout of 5 seconds
        EventBits_t bits = xEventGroupWaitBits(s_mqtt_event_group,
                                               MQTT_CONNECTED_BIT,
                                               pdFALSE,
                                               pdFALSE,
                                               pdMS_TO_TICKS(MQTT_EVENT_GROUPS_WAIT_BITS_TIMEOUT_MS));

        if (bits & MQTT_CONNECTED_BIT) {
            ESP_LOGI(TAG, "MQTT Conectado com Sucesso!");
            return true;
        }

        esp_mqtt_client_stop(mqtt_client);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGE(TAG, "Esgotadas as %d tentativas de conexão com o Broker MQTT.", max_retries);
    return false;
}

bool mqtt_publish_status(const char *json_payload, bool retain) {
    if (!mqtt_is_connected()) return false;

    char topic[64];
    snprintf(topic, sizeof(topic), "explorer/command/%s", s_mac_str);

    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, json_payload, 0, 1, retain ? 1 : 0);
    return (msg_id != -1);
}

bool mqtt_publish_commands_json(const char *json_payload, char *out_mac_str) {
    if (!wifi_is_connected() || !mqtt_is_connected()) return false;

    if (out_mac_str) strcpy(out_mac_str, s_mac_str);

    char topic[64];
    snprintf(topic, sizeof(topic), "explorer/command/%s", s_mac_str);

    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, json_payload, 0, 1, 0);
    return (msg_id != -1);
}