/**
 * @file json_protocol.c
 * @brief Implementation of JSON Encoders using cJSON
 */

#include "json_protocol.h"
#include "cJSON.h"
#include "esp_log.h"
#include <stdlib.h>

static const char *TAG = "JSON_PROTOCOL";

esp_err_t json_encode_telemetry(const telemetry_data_t *data, char **out_str) {
    if (data == NULL || out_str == NULL) {
        ESP_LOGE(TAG, "Invalid argument: NULL pointer provided");
        return ESP_ERR_INVALID_ARG;
    }

    // Validate action range
    last_action_t action = data->last_action;
    if (action < ACTION_TELEMETRY || action > ACTION_SET_TEMP_25) {
        ESP_LOGW(TAG, "Out of range last_action (%d), defaulting to ACTION_TELEMETRY", action);
        action = ACTION_TELEMETRY;
    }

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to allocate cJSON root object");
        return ESP_ERR_NO_MEM;
    }

    // Check if the string buffer starts with a null terminator
    const char *rtc_val = (data->rtc_time[0] != '\0') ? data->rtc_time : "00:00";
    const char *rssi_val = (data->rssi[0] != '\0') ? data->rssi : "0";

    double bat_rounded = (int)(data->battery_voltage * 100.0f + (data->battery_voltage >= 0 ? 0.5f : -0.5f)) / 100.0;

    cJSON_AddNumberToObject(root, "cmd_id", 0);
    cJSON_AddNumberToObject(root, "temp", data->temperature);
    cJSON_AddNumberToObject(root, "umid", data->humidity);
    cJSON_AddStringToObject(root, "rtc", rtc_val);
    cJSON_AddStringToObject(root, "RSSI", rssi_val);
    cJSON_AddNumberToObject(root, "bat", bat_rounded);
    cJSON_AddNumberToObject(root, "last_action", (int)action);

    *out_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (*out_str == NULL) {
        ESP_LOGE(TAG, "Failed to render JSON string");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t json_encode_wifi_ack(const wifi_ack_payload_t *ack, char **out_str) {
    if (ack == NULL || out_str == NULL) {
        ESP_LOGE(TAG, "Invalid argument: NULL pointer provided");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to allocate cJSON root object");
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddNumberToObject(root, "cmd_id", 4);
    cJSON_AddStringToObject(root, "ssid", ack->ssid ? ack->ssid : "");
    cJSON_AddStringToObject(root, "password", ack->password ? ack->password : "");

    *out_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (*out_str == NULL) {
        ESP_LOGE(TAG, "Failed to render JSON string");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t json_encode_schedule_ack(const schedule_ack_payload_t *ack, char **out_str) {
    if (ack == NULL || out_str == NULL) {
        ESP_LOGE(TAG, "Invalid argument: NULL pointer provided");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to allocate cJSON root object");
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddNumberToObject(root, "cmd_id", 6);
    cJSON_AddNumberToObject(root, "week_days", ack->week_days);
    cJSON_AddStringToObject(root, "time", ack->time ? ack->time : "00:00");
    cJSON_AddStringToObject(root, "action", ack->action ? ack->action : "");
    cJSON_AddStringToObject(root, "status", ack->status ? ack->status : "OK");

    *out_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (*out_str == NULL) {
        ESP_LOGE(TAG, "Failed to render JSON string");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

char* build_telemetry_json(int temperature, 
                           int humidity, 
                           const char *rtc_time, 
                           const char *rssi, 
                           float battery_voltage, 
                           last_action_t last_action) 
{
    telemetry_data_t telemetria = {
        .temperature = temperature,
        .humidity = humidity,
        .battery_voltage = battery_voltage,
        .last_action = last_action
    };

    // Safe string copy to avoid buffer overflow
    if (rtc_time != NULL) {
        snprintf(telemetria.rtc_time, sizeof(telemetria.rtc_time), "%s", rtc_time);
    } else {
        snprintf(telemetria.rtc_time, sizeof(telemetria.rtc_time), "00:00");
    }

    if (rssi != NULL) {
        snprintf(telemetria.rssi, sizeof(telemetria.rssi), "%s", rssi);
    } else {
        snprintf(telemetria.rssi, sizeof(telemetria.rssi), "0");
    }

    char *json_payload = NULL;
    esp_err_t err = json_encode_telemetry(&telemetria, &json_payload);

    if (err != ESP_OK || json_payload == NULL) {
        ESP_LOGE("JSON_UTIL", "Erro ao gerar JSON de telemetria: %s", esp_err_to_name(err));
        return NULL;
    }

    return json_payload;
}