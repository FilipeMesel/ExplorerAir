#include "wifi_handler.h"
#include "config.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "WIFI_HANDLER";

#define WIFI_CONNECTED_BIT BIT0

static EventGroupHandle_t s_wifi_event_group = NULL;
static bool wifi_is_initialized = false;
static char s_mac_str[13] = {0};

static int s_nvs_retry_count = 0;
static bool s_using_fallback = false;
static bool s_connection_failed_permanently = false;

bool is_wifi_initialized(void) {
    return wifi_is_initialized;
}

bool wifi_is_connected(void) {
    if (s_wifi_event_group == NULL) return false;
    EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
    return (bits & WIFI_CONNECTED_BIT) != 0;
}

bool wifi_is_failed(void) {
    return s_connection_failed_permanently;
}

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Wi-Fi Desconectado.");
        if (s_wifi_event_group) {
            xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "IP Obtido: " IPSTR, IP2STR(&event->ip_info.ip));
        if (s_wifi_event_group) {
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}

bool wifi_save_nvs_credentials(const char *ssid, const char *pass) {
    nvs_handle_t nvs_h;
    esp_err_t err = nvs_open(NVS_WIFI_NAMESPACE, NVS_READWRITE, &nvs_h);
    if (err != ESP_OK) return false;

    nvs_set_str(nvs_h, NVS_KEY_SSID, ssid);
    nvs_set_str(nvs_h, NVS_KEY_PASS, pass);
    err = nvs_commit(nvs_h);
    nvs_close(nvs_h);

    ESP_LOGI(TAG, "Novas credenciais salvas na NVS (SSID: %s)", ssid);
    return (err == ESP_OK);
}

static bool wifi_get_nvs_credentials(char *ssid_out, char *pass_out) {
    nvs_handle_t nvs_h;
    if (nvs_open(NVS_WIFI_NAMESPACE, NVS_READONLY, &nvs_h) != ESP_OK) return false;

    size_t ssid_len = 32, pass_len = 64;
    esp_err_t err1 = nvs_get_str(nvs_h, NVS_KEY_SSID, ssid_out, &ssid_len);
    esp_err_t err2 = nvs_get_str(nvs_h, NVS_KEY_PASS, pass_out, &pass_len);
    nvs_close(nvs_h);

    return (err1 == ESP_OK && err2 == ESP_OK);
}

void wifi_reset_connection_state(void) {
    s_nvs_retry_count = 0;
    s_using_fallback = false;
    s_connection_failed_permanently = false;
    if (s_wifi_event_group) {
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
    esp_wifi_stop();
}

void wifi_connect_init(wifi_status_cb_t status_cb) {
    if (s_connection_failed_permanently) return;

    if (s_wifi_event_group == NULL) {
        s_wifi_event_group = xEventGroupCreate();
    }

    if (wifi_is_connected()) return;

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    sprintf(s_mac_str, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    if (!wifi_is_initialized) {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_sta();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL));

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        wifi_is_initialized = true;
    }

    wifi_config_t wifi_config = {0};
    char nvs_ssid[32] = {0}, nvs_pass[64] = {0};
    char msg_buf[64];

    if (!s_using_fallback && wifi_get_nvs_credentials(nvs_ssid, nvs_pass)) {
        strncpy((char*)wifi_config.sta.ssid, nvs_ssid, sizeof(wifi_config.sta.ssid));
        strncpy((char*)wifi_config.sta.password, nvs_pass, sizeof(wifi_config.sta.password));
        
        // Linha 1: CONECTANDO | Linha 2: SSID NVS
        if (status_cb) status_cb("CONECTANDO", nvs_ssid);

        ESP_LOGI(TAG, "Tentando NVS Wi-Fi: %s (Tentativa %d/3)", nvs_ssid, s_nvs_retry_count + 1);
    } else {
        s_using_fallback = true;
        strncpy((char*)wifi_config.sta.ssid, WIFI_FALLBACK_SSID, sizeof(wifi_config.sta.ssid));
        strncpy((char*)wifi_config.sta.password, WIFI_FALLBACK_PASS, sizeof(wifi_config.sta.password));
        
        // Linha 1: CONECTANDO | Linha 2: SSID Fallback
        if (status_cb) status_cb("CONECTANDO", WIFI_FALLBACK_SSID);

        ESP_LOGW(TAG, "Tentando Fallback Wi-Fi: %s", WIFI_FALLBACK_SSID);
    }

    esp_wifi_stop();
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(5000));

    if (bits & WIFI_CONNECTED_BIT) {
        s_nvs_retry_count = 0;
    } else {
        ESP_LOGE(TAG, "Falha ao conectar na rede Wi-Fi.");

        if (!s_using_fallback) {
            s_nvs_retry_count++;
            if (s_nvs_retry_count >= 3) {
                ESP_LOGE(TAG, "3 Falhas na NVS. Trocando para Fallback (%s)...", WIFI_FALLBACK_SSID);
                s_using_fallback = true;
            }
        } else {
            ESP_LOGE(TAG, "Fallback falhou. Entrando no modo ERRO WIFI.");
            s_connection_failed_permanently = true;
            esp_wifi_stop();
        }
    }
}