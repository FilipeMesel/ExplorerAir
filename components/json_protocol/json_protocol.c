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

    cJSON_AddNumberToObject(root, "cmd_id", CMD_ID_WIFI_ACK);
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

    cJSON_AddNumberToObject(root, "cmd_id", CMD_ID_SCHEDULE_ACK);
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

esp_err_t json_decode_cmd1_rtc_sync(const char *json_str, cmd1_rtc_sync_payload_t *payload) {
    if (json_str == NULL || payload == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(payload, 0, sizeof(cmd1_rtc_sync_payload_t));

    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) {
        ESP_LOGE(TAG, "CMD 1: JSON invalido");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *interval = cJSON_GetObjectItemCaseSensitive(root, "telemetry_update");
    cJSON *actual_time = cJSON_GetObjectItemCaseSensitive(root, "actual_time");

    if (!cJSON_IsNumber(interval) || !cJSON_IsString(actual_time) || (actual_time->valuestring == NULL)) {
        ESP_LOGE(TAG, "CMD 1: Campos 'telemetry_update' ou 'actual_time' ausentes/invalidos");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    payload->interval_sec = (uint16_t)interval->valueint;

    // Formated String parse "HH:MM:SS"
    int hour = 0, minute = 0, second = 0;
    if (sscanf(actual_time->valuestring, "%d:%d:%d", &hour, &minute, &second) != 3) {
        ESP_LOGE(TAG, "CMD 1: Formato invalido para actual_time ('%s'). Esperado 'HH:MM:SS'", actual_time->valuestring);
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    payload->hour = (uint8_t)hour;
    payload->minute = (uint8_t)minute;
    payload->second = (uint8_t)second;

    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t json_decode_cmd4_wifi_prov(const char *json_str, cmd4_wifi_prov_payload_t *payload) {
    if (json_str == NULL || payload == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) {
        ESP_LOGE(TAG, "CMD 4: JSON invalido");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *ssid = cJSON_GetObjectItemCaseSensitive(root, "ssid");
    cJSON *pass = cJSON_GetObjectItemCaseSensitive(root, "password");

    if (!cJSON_IsString(ssid) || (ssid->valuestring == NULL) ||
        !cJSON_IsString(pass) || (pass->valuestring == NULL)) {
        ESP_LOGE(TAG, "CMD 4: Estrutura de ssid/password invalida");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    // Security copy to avoid buffer overflow
    snprintf(payload->ssid, sizeof(payload->ssid), "%s", ssid->valuestring);
    snprintf(payload->password, sizeof(payload->password), "%s", pass->valuestring);

    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t json_decode_cmd6_schedule(const char *json_str, cmd6_schedule_payload_t *payload) {
    if (json_str == NULL || payload == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(payload, 0, sizeof(cmd6_schedule_payload_t));

    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) {
        ESP_LOGE(TAG, "CMD 6: JSON invalido");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *idx = cJSON_GetObjectItemCaseSensitive(root, "schedule_id");
    cJSON *wd = cJSON_GetObjectItemCaseSensitive(root, "week_days");
    cJSON *tm = cJSON_GetObjectItemCaseSensitive(root, "time");
    cJSON *act = cJSON_GetObjectItemCaseSensitive(root, "action");

    if (!cJSON_IsNumber(idx) || !cJSON_IsNumber(wd) || 
        !cJSON_IsString(tm)  || (tm->valuestring == NULL) ||
        !cJSON_IsString(act) || (act->valuestring == NULL)) {
        ESP_LOGE(TAG, "CMD 5: Campos obrigatorios ausentes ou invalidos");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    // Validate schedule_id range (0 to 10)
    if (idx->valueint < 0 || idx->valueint >= MAX_SCHEDULES) {
        ESP_LOGE(TAG, "CMD 6: schedule_id fora dos limites (0-10): %d", idx->valueint);
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    schedule_item_t *sch = &payload->items[0];
    sch->index = (uint8_t)idx->valueint;
    sch->week_days = (uint8_t)wd->valueint; //e.g. 62 -> 0b00111110
    snprintf(sch->time, sizeof(sch->time), "%s", tm->valuestring);
    snprintf(sch->action, sizeof(sch->action), "%s", act->valuestring);

    payload->count = 1;
    cJSON_Delete(root);

    return ESP_OK;
}

// ----------------------------------------------------------------------------
// CMD 8 Parser - Set IR Raw Data
// ----------------------------------------------------------------------------
esp_err_t json_decode_cmd8_ir_raw(const char *json_str, cmd8_ir_raw_payload_t *payload) {
    if (json_str == NULL || payload == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(payload, 0, sizeof(cmd8_ir_raw_payload_t));

    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) {
        ESP_LOGE(TAG, "CMD 8: JSON invalido");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *action = cJSON_GetObjectItemCaseSensitive(root, "action");
    cJSON *length = cJSON_GetObjectItemCaseSensitive(root, "length");
    cJSON *raw_data = cJSON_GetObjectItemCaseSensitive(root, "raw_data");

    if (!cJSON_IsNumber(action) || !cJSON_IsNumber(length) || !cJSON_IsArray(raw_data)) {
        ESP_LOGE(TAG, "CMD 8: Campos 'action', 'length' ou 'raw_data' ausentes/invalidos");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    payload->action = (uint8_t)action->valueint;
    payload->length = (uint16_t)length->valueint;

    uint16_t count = 0;
    cJSON *item = NULL;

    cJSON_ArrayForEach(item, raw_data) {
        if (count >= MAX_IR_RAW_TIMINGS) {
            ESP_LOGW(TAG, "CMD 8: Limite de %d timings excedido, ignorando restantes", MAX_IR_RAW_TIMINGS);
            break;
        }

        if (cJSON_IsNumber(item)) {
            payload->raw_data[count++] = (uint16_t)item->valueint;
        }
    }

    payload->raw_data_count = count;
    cJSON_Delete(root);

    if (count == 0) {
        ESP_LOGE(TAG, "CMD 8: Nenhum timing valido extraido de 'raw_data'");
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

// ----------------------------------------------------------------------------
// CMD 9 Encoder - Set IR Raw Data ACK
// ----------------------------------------------------------------------------
esp_err_t json_encode_ir_raw_ack(const cmd9_ir_raw_ack_payload_t *ack, char **out_str) {
    if (ack == NULL || out_str == NULL) {
        ESP_LOGE(TAG, "Invalid argument: NULL pointer provided");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to allocate cJSON root object");
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddNumberToObject(root, "cmd_id", CMD_ID_SET_IR_RAW_DATA_ACK);
    cJSON_AddStringToObject(root, "status", ack->status ? ack->status : "OK");
    cJSON_AddNumberToObject(root, "count_received", ack->count_received);

    *out_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (*out_str == NULL) {
        ESP_LOGE(TAG, "Failed to render JSON string");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}