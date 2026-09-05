/**
 * @file board_mqtt.c
 * @brief MQTT Client Engine Implementation
 */

#include <stdio.h>
#include <string.h>
#include "esp_mac.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "board_mqtt.h"

static const char *TAG = "BOARD_MQTT";

ESP_EVENT_DEFINE_BASE(BOARD_MQTT_EVENTS);

static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static char s_uplink_topic[128] = {0};
static char s_downlink_topic[128] = {0};

static void generate_mac_topics(void) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    
    char mac_str[13];
    snprintf(mac_str, sizeof(mac_str), "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    snprintf(s_uplink_topic, sizeof(s_uplink_topic), "explorerIRBlaster/%s/UPLINK", mac_str);
    snprintf(s_downlink_topic, sizeof(s_downlink_topic), "explorerIRBlaster/%s/DOWNLINK", mac_str);

    ESP_LOGI(TAG, "UPLINK Topic: %s", s_uplink_topic);
    ESP_LOGI(TAG, "DOWNLINK Topic: %s", s_downlink_topic);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT Connected. Subscribing to Downlink...");
            ESP_LOGI(TAG, "Subscribing to topic: %s", s_downlink_topic);
            esp_mqtt_client_subscribe(s_mqtt_client, s_downlink_topic, 1);
            esp_event_post(BOARD_MQTT_EVENTS, BOARD_MQTT_EVENT_CONNECTED, NULL, 0, portMAX_DELAY);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT Disconnected.");
            esp_event_post(BOARD_MQTT_EVENTS, BOARD_MQTT_EVENT_DISCONNECTED, NULL, 0, portMAX_DELAY);
            break;

        case MQTT_EVENT_DATA: {
            board_mqtt_data_t incoming_data = {0};
            snprintf(incoming_data.topic, sizeof(incoming_data.topic), "%.*s", event->topic_len, event->topic);
            snprintf(incoming_data.payload, sizeof(incoming_data.payload), "%.*s", event->data_len, event->data);
            incoming_data.payload_len = event->data_len;

            esp_event_post(BOARD_MQTT_EVENTS, BOARD_MQTT_EVENT_DATA_RECEIVED, &incoming_data, sizeof(board_mqtt_data_t), portMAX_DELAY);
            break;
        }
        default:
            break;
    }
}

esp_err_t board_mqtt_init(const char *broker_uri, int buffer_size, int out_buffer_size) {
    generate_mac_topics();

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = broker_uri,
        .buffer.size = buffer_size,
        .buffer.out_size = out_buffer_size,
    };

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!s_mqtt_client) return ESP_FAIL;

    return esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
}

esp_err_t board_mqtt_start(void) {
    if (!s_mqtt_client) return ESP_ERR_INVALID_STATE;
    return esp_mqtt_client_start(s_mqtt_client);
}

int board_mqtt_publish_uplink(const char *json_payload, int qos) {
    if (!s_mqtt_client || !json_payload) return -1;
    return esp_mqtt_client_publish(s_mqtt_client, s_uplink_topic, json_payload, 0, qos, 0);
}

esp_err_t board_mqtt_stop(void) {
    if (!s_mqtt_client) return ESP_OK;
    esp_mqtt_client_stop(s_mqtt_client);
    return esp_mqtt_client_destroy(s_mqtt_client);
}