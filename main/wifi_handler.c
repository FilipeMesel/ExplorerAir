/**
 * @file wifi_handler.c
 * @author Filipe Mesel Lobo Costa Cardoso
 * @brief This file contains the implementation of Wi-Fi communication handling for the Explorer IR Blaster project.
 * @version 0.1
 * @date 2026-08-31
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include "wifi_handler.h"
#include "config.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "explorer_memory.h"

static const char *TAG = "WIFI_HANDLER";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define MAX_RETRY_PER_NET  3                            /** Maximum number of retry attempts per network */

static EventGroupHandle_t s_wifi_event_group = NULL;    /** Event group for Wi-Fi connection status */
static esp_netif_t *s_netif_sta = NULL;                 /** Wi-Fi station interface */
static wifi_status_cb_t s_status_cb = NULL;             /** Wi-Fi status callback function */

static int s_retry_count = 0;                           /** Retry count for Wi-Fi connection attempts */
static bool s_is_connected = false;                     /** Wi-Fi connection status */
static bool s_is_failed = false;                        /** Wi-Fi connection failure status */

static void event_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_retry_count++;
        if (s_retry_count < MAX_RETRY_PER_NET) {
            ESP_LOGW(TAG, "Falha ao conectar. Tentativa %d/%d...", s_retry_count + 1, MAX_RETRY_PER_NET);
            esp_wifi_connect();
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "IP Obtido: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_count = 0;
        s_is_connected = true;
        s_is_failed = false;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static bool wifi_load_credentials(char *ssid, char *pass) {
    return explorer_memory_load_wifi_credentials(ssid, 32, pass, 64);
}

bool wifi_save_credentials(const char *ssid, const char *password) {
    return explorer_memory_save_wifi_credentials(ssid, password);
}

static bool try_connect_network(const char *ssid, const char *pass, const char *net_label) {
    s_retry_count = 0;
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_LOGI(TAG, "Tentando conectar na rede [%s]: %s", net_label, ssid);
    
    if (s_status_cb) {
        char buf[32];
        snprintf(buf, sizeof(buf), "CONECTANDO...");
        s_status_cb(net_label, buf);
    }

    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Conectado com sucesso em [%s]!", ssid);
        return true;
    }

    ESP_LOGE(TAG, "Falha de conexão na rede [%s].", ssid);
    return false;
}

void wifi_connect_init(wifi_status_cb_t cb) {
    s_status_cb = cb;
    s_is_connected = false;
    s_is_failed = false;

    if (s_wifi_event_group == NULL) {
        s_wifi_event_group = xEventGroupCreate();
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        s_netif_sta = esp_netif_create_default_wifi_sta();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                            ESP_EVENT_ANY_ID,
                                                            &event_handler,
                                                            NULL,
                                                            NULL));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                            IP_EVENT_STA_GOT_IP,
                                                            &event_handler,
                                                            NULL,
                                                            NULL));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    }

    char client_ssid[32] = {0};
    char client_pass[64] = {0};
    bool has_client_credentials = wifi_load_credentials(client_ssid, client_pass);

    // 1. Try 3 times on the client's network (if credentials are saved)
    if (has_client_credentials && strlen(client_ssid) > 0) {
        if (try_connect_network(client_ssid, client_pass, "WIFI CLIENTE")) {
            return;
        }
    } else {
        ESP_LOGW(TAG, "Sem credenciais do cliente presas na NVS. Pulando para Fallback...");
    }

    // 2. Try 3 times on each of the N fallback networks defined in config.h
    for (size_t i = 0; i < NUM_FALLBACK_NETWORKS; i++) {
        char label_buf[32];
        snprintf(label_buf, sizeof(label_buf), "FALLBACK %u", (unsigned int)(i + 1));

        if (try_connect_network(WIFI_FALLBACK_NETWORKS[i].ssid, WIFI_FALLBACK_NETWORKS[i].pass, label_buf)) {
            return;
        }
    }

    // I fail the connection and set the failure status
    s_is_failed = true;
    s_is_connected = false;
    ESP_LOGE(TAG, "Todas as tentativas de Wi-Fi falharam.");
    if (s_status_cb) {
        s_status_cb("ERRO WIFI", "FALHA CONEXAO");
    }
}

bool wifi_is_connected(void) {
    return s_is_connected;
}

bool wifi_is_failed(void) {
    return s_is_failed;
}

int8_t wifi_get_rssi(void) {
    if (!s_is_connected) return 0;
    
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        return ap_info.rssi;
    }
    return 0;
}