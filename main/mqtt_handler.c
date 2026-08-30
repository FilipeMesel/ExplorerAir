#include "mqtt_handler.h"
#include "wifi_handler.h"
#include "config.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <string.h>

static const char *TAG = "MQTT_HANDLER";

#define MQTT_CONNECTED_BIT BIT0

static EventGroupHandle_t s_mqtt_event_group = NULL;
static esp_mqtt_client_handle_t mqtt_client = NULL;
static char s_mac_str[13] = {0};

extern void protocol_parse_payload(const char *payload);

bool mqtt_is_connected(void) {
    if (s_mqtt_event_group == NULL) return false;
    EventBits_t bits = xEventGroupGetBits(s_mqtt_event_group);
    return (bits & MQTT_CONNECTED_BIT) != 0;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Conectado ao Broker MQTT!");
            if (s_mqtt_event_group) {
                xEventGroupSetBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
            }
            
            char sub_topic[64];
            snprintf(sub_topic, sizeof(sub_topic), "explorer/device/%s/set", s_mac_str);
            esp_mqtt_client_subscribe(mqtt_client, sub_topic, 1);
            break;

        case MQTT_EVENT_DATA:
            if (event->data_len > 0) {
                char *payload_buf = malloc(event->data_len + 1);
                if (payload_buf) {
                    memcpy(payload_buf, event->data, event->data_len);
                    payload_buf[event->data_len] = '\0';
                    protocol_parse_payload(payload_buf);
                    free(payload_buf);
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

void mqtt_init(void) {
    if (!wifi_is_connected()) {
        ESP_LOGE(TAG, "Impossível inicializar MQTT sem conexão Wi-Fi!");
        return;
    }

    if (s_mqtt_event_group == NULL) {
        s_mqtt_event_group = xEventGroupCreate();
    }

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    sprintf(s_mac_str, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    if (mqtt_client == NULL) {
        esp_mqtt_client_config_t mqtt_cfg = { .broker.address.uri = MQTT_BROKER_URI };
        mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
        esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
        esp_mqtt_client_start(mqtt_client);
    }
}

bool mqtt_publish_commands_json(const char *json_payload, char *mac_str_out) {
    if (!wifi_is_connected() || !mqtt_is_connected()) {
        ESP_LOGE(TAG, "Falha na publicação: Wi-Fi ou MQTT desconectados.");
        return false;
    }

    if (mac_str_out) {
        strcpy(mac_str_out, s_mac_str);
    }

    char topic[64];
    snprintf(topic, sizeof(topic), "explorer/command/%s", s_mac_str);

    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, json_payload, 0, 1, 0);
    return (msg_id != -1);
}