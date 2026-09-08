/**
 * @file json_protocol.c
 * @brief Implementation of JSON Encoders/Decoders using cJSON
 */

#include "json_protocol.h"
#include "cJSON.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "JSON_PROTOCOL";

esp_err_t json_encode_telemetry(const telemetry_data_t *data, char *out_buf, size_t max_len) {
    if (data == NULL || out_buf == NULL || max_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (data->last_action < LAST_ACTION_NONE || data->last_action > ACTION_SET_TEMP_25) {
        return ESP_ERR_INVALID_ARG;
    }

    float bat_v = data->battery_mv / 1000.0f;

    int len = snprintf(out_buf, max_len,
        "{\"cmd_id\":0,\"temp\":%d,\"umid\":%d,\"rtc\":\"%02d:%02d:%02d\",\"day\":%d,\"month\":%d,\"year\":%d,\"weekday\":%d,\"rssi\":%d,\"bat\":%.2f,\"last_action\":%d}",
        data->temp, 
        data->umid, 
        data->sync_time_t.hour, 
        data->sync_time_t.minute,
        data->sync_time_t.second,
        data->sync_time_t.day,
        data->sync_time_t.month,
        data->sync_time_t.year,
        data->sync_time_t.weekday, 
        data->rssi, 
        bat_v, 
        (int)data->last_action);

    if (len < 0 || (size_t)len >= max_len) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t json_get_cmd_id(const char *json_str, int *cmd_id) {
    if (json_str == NULL || cmd_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) {
        ESP_LOGE(TAG, "Falha no parse do JSON");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, "cmd_id");
    if (!cJSON_IsNumber(item)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    *cmd_id = item->valueint;
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t json_decode_sync(const char *json_str, cmd1_sync_data_t *out_data) {
    if (json_str == NULL || out_data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) {
        ESP_LOGE(TAG, "Falha ao realizar parse do JSON no CMD 1");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *cmd_id           = cJSON_GetObjectItemCaseSensitive(root, "cmd_id");
    cJSON *hour             = cJSON_GetObjectItemCaseSensitive(root, "hour");
    cJSON *min              = cJSON_GetObjectItemCaseSensitive(root, "min");
    cJSON *sec              = cJSON_GetObjectItemCaseSensitive(root, "sec");
    cJSON *week_day         = cJSON_GetObjectItemCaseSensitive(root, "week_day");
    cJSON *telemetry_update = cJSON_GetObjectItemCaseSensitive(root, "telemetry_update");

    if (!cJSON_IsNumber(cmd_id) || !cJSON_IsNumber(hour) || !cJSON_IsNumber(min) ||
        !cJSON_IsNumber(sec) || !cJSON_IsNumber(week_day) || !cJSON_IsNumber(telemetry_update)) {
        ESP_LOGE(TAG, "Campos invalidos ou ausentes no JSON do CMD 1");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    out_data->cmd_id                   = cmd_id->valueint;
    out_data->sync_time_t.hour         = (uint8_t)hour->valueint;
    out_data->sync_time_t.minute       = (uint8_t)min->valueint;
    out_data->sync_time_t.second       = (uint8_t)sec->valueint;
    out_data->sync_time_t.weekday      = (uint8_t)week_day->valueint;
    out_data->sync_time_t.day          = 1;    // Valor default
    out_data->sync_time_t.month        = 1;    // Valor default
    out_data->sync_time_t.year         = 2026; // Valor default
    out_data->telemetry_update         = telemetry_update->valueint;

    cJSON_Delete(root);
    return ESP_OK;
}