/**
 * @file board_wifi.c
 * @brief Wi-Fi Failover Manager Implementation
 */

#include "board_wifi.h"
#include <string.h>
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "BOARD_WIFI";

ESP_EVENT_DEFINE_BASE(BOARD_WIFI_EVENTS);

// Static N-1 known networks
static const wifi_credential_t g_known_networks[] = {
    {.ssid = "conectaSenfio", .password = "12345678"},
    {.ssid = "Senfio_Lab",    .password = "senfio2026"}
};
#define KNOWN_NETWORKS_COUNT (sizeof(g_known_networks) / sizeof(wifi_credential_t))

static wifi_credential_t g_dynamic_credential = {0};
static bool g_has_dynamic_cred = false;

static uint8_t g_current_net_index = 0; // 0 = Dynamic, 1..N = Static
static uint8_t g_retry_count = 0;
static esp_netif_t *g_netif_sta = NULL;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        g_retry_count++;
        if (g_retry_count < WIFI_MAX_FAILOVER_RETRIES) {
            ESP_LOGW(TAG, "Retrying connection (%d/%d)...", g_retry_count, WIFI_MAX_FAILOVER_RETRIES);
            esp_wifi_connect();
        } else {
            ESP_LOGW(TAG, "Failed 3 times. Switching to next network target...");
            g_retry_count = 0;
            g_current_net_index++;

            // Total networks = 1 (Dynamic if present) + KNOWN_NETWORKS_COUNT
            uint8_t total_nets = (g_has_dynamic_cred ? 1 : 0) + KNOWN_NETWORKS_COUNT;

            if (g_current_net_index >= total_nets) {
                ESP_LOGE(TAG, "Exhausted all available Wi-Fi networks.");
                esp_event_post(BOARD_WIFI_EVENTS, BOARD_WIFI_EVENT_FAILOVER_EXHAUSTED, NULL, 0, portMAX_DELAY);
            } else {
                // Configure next target network
                wifi_config_t wifi_cfg = {0};
                const wifi_credential_t *target_cred = NULL;

                if (g_has_dynamic_cred && g_current_net_index == 0) {
                    target_cred = &g_dynamic_credential;
                } else {
                    uint8_t static_idx = g_has_dynamic_cred ? (g_current_net_index - 1) : g_current_net_index;
                    target_cred = &g_known_networks[static_idx];
                }

                strncpy((char *)wifi_cfg.sta.ssid, target_cred->ssid, sizeof(wifi_cfg.sta.ssid));
                strncpy((char *)wifi_cfg.sta.password, target_cred->password, sizeof(wifi_cfg.sta.password));

                ESP_LOGI(TAG, "Attempting connection to SSID: %s", wifi_cfg.sta.ssid);
                esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
                esp_wifi_connect();
            }
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Successfully connected! IP:" IPSTR, IP2STR(&event->ip_info.ip));
        esp_event_post(BOARD_WIFI_EVENTS, BOARD_WIFI_EVENT_CONNECTED, NULL, 0, portMAX_DELAY);
    }
}

esp_err_t board_wifi_init(void) {
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK) return ret;

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) return ret;

    g_netif_sta = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) return ret;

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    return esp_wifi_set_mode(WIFI_MODE_STA);
}

esp_err_t board_wifi_set_dynamic_credential(const wifi_credential_t *cred) {
    if (!cred) return ESP_ERR_INVALID_ARG;
    memcpy(&g_dynamic_credential, cred, sizeof(wifi_credential_t));
    g_has_dynamic_cred = (strlen(g_dynamic_credential.ssid) > 0);
    return ESP_OK;
}

esp_err_t board_wifi_start_failover_connect(void) {
    g_current_net_index = 0;
    g_retry_count = 0;

    wifi_config_t wifi_cfg = {0};
    const wifi_credential_t *first_cred = NULL;

    if (g_has_dynamic_cred) {
        first_cred = &g_dynamic_credential;
    } else {
        first_cred = &g_known_networks[0];
    }

    strncpy((char *)wifi_cfg.sta.ssid, first_cred->ssid, sizeof(wifi_cfg.sta.ssid));
    strncpy((char *)wifi_cfg.sta.password, first_cred->password, sizeof(wifi_cfg.sta.password));

    ESP_LOGI(TAG, "Starting Failover Sequence. Primary SSID: %s", wifi_cfg.sta.ssid);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    return esp_wifi_start();
}

esp_err_t board_wifi_stop(void) {
    esp_wifi_disconnect();
    return esp_wifi_stop();
}