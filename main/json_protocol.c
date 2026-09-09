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

esp_err_t json_decode_sync(const char *json_str, cmd1_sync_data_t *out_data)
{
    if (json_str == NULL || out_data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Leitura dos campos do JSON
    cJSON *hour             = cJSON_GetObjectItemCaseSensitive(root, "hour");
    cJSON *minute           = cJSON_GetObjectItemCaseSensitive(root, "min");
    cJSON *second           = cJSON_GetObjectItemCaseSensitive(root, "sec");
    cJSON *weekday          = cJSON_GetObjectItemCaseSensitive(root, "weekday");
    cJSON *day              = cJSON_GetObjectItemCaseSensitive(root, "day");
    cJSON *month            = cJSON_GetObjectItemCaseSensitive(root, "month");
    cJSON *year             = cJSON_GetObjectItemCaseSensitive(root, "year");
    cJSON *telemetry_update = cJSON_GetObjectItemCaseSensitive(root, "telemetry_update");

    // Validação dos campos obrigatórios do horário
    if (!cJSON_IsNumber(hour) || !cJSON_IsNumber(minute) || !cJSON_IsNumber(second)) {
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    // Preenchimento do horário e dia da semana
    out_data->sync_time_t.hour    = (uint8_t)hour->valueint;
    out_data->sync_time_t.minute  = (uint8_t)minute->valueint;
    out_data->sync_time_t.second  = (uint8_t)second->valueint;
    out_data->sync_time_t.weekday = cJSON_IsNumber(weekday) ? (uint8_t)weekday->valueint : 0;

    // Preenchimento da data (com fallback para 0 caso ausente)
    out_data->sync_time_t.day     = cJSON_IsNumber(day)   ? (uint8_t)day->valueint   : 0;
    out_data->sync_time_t.month   = cJSON_IsNumber(month) ? (uint8_t)month->valueint : 0;
    out_data->sync_time_t.year    = cJSON_IsNumber(year)  ? (uint16_t)year->valueint : 0;

    // Decodificação do novo campo: telemetry_update (em segundos)
    if (cJSON_IsNumber(telemetry_update)) {
        out_data->telemetry_update = telemetry_update->valueint;
    } else {
        // Valor padrão de segurança (ex: 300s = 5 min) se o campo não vier no JSON
        out_data->telemetry_update = DEFAULT_UPDATE_TIME;
    }

    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t json_decode_wifi_prov(const char *json_str, wifi_prov_payload_t *out_payload) {
    if (json_str == NULL || out_payload == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) {
        ESP_LOGE(TAG, "Erro ao realizar parse do JSON no CMD 4");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *ssid = cJSON_GetObjectItemCaseSensitive(root, "ssid");
    cJSON *pass = cJSON_GetObjectItemCaseSensitive(root, "password");

    if (!cJSON_IsString(ssid) || (ssid->valuestring == NULL) ||
        !cJSON_IsString(pass) || (pass->valuestring == NULL)) {
        ESP_LOGE(TAG, "Campos 'ssid' ou 'password' inválidos/ausentes no CMD 4");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    memset(out_payload, 0, sizeof(wifi_prov_payload_t));
    strncpy(out_payload->ssid, ssid->valuestring, WIFI_SSID_MAX_LEN - 1);
    strncpy(out_payload->password, pass->valuestring, WIFI_PASS_MAX_LEN - 1);

    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t json_encode_wifi_ack(const wifi_prov_payload_t *payload, char *pub_buf, size_t max_len) {
    if (payload == NULL || pub_buf == NULL || max_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    int len = snprintf(pub_buf, max_len,
        "{\"cmd_id\":5,\"ssid\":\"%s\",\"password\":\"%s\"}",
        payload->ssid,
        payload->password);

    if (len < 0 || (size_t)len >= max_len) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

static last_action_t action_str_to_enum(const char *str) {
    if (!str) return LAST_ACTION_NONE;
    if (strcmp(str, "POWER_OFF") == 0) return ACTION_POWER_OFF;
    if (strcmp(str, "POWER_ON") == 0) return ACTION_POWER_ON;
    if (strcmp(str, "SET_TEMP_18") == 0) return ACTION_SET_TEMP_18;
    if (strcmp(str, "SET_TEMP_19") == 0) return ACTION_SET_TEMP_19;
    if (strcmp(str, "SET_TEMP_20") == 0) return ACTION_SET_TEMP_20;
    if (strcmp(str, "SET_TEMP_21") == 0) return ACTION_SET_TEMP_21;
    if (strcmp(str, "SET_TEMP_22") == 0) return ACTION_SET_TEMP_22;
    if (strcmp(str, "SET_TEMP_23") == 0) return ACTION_SET_TEMP_23;
    if (strcmp(str, "SET_TEMP_24") == 0) return ACTION_SET_TEMP_24;
    if (strcmp(str, "SET_TEMP_25") == 0) return ACTION_SET_TEMP_25;
    return LAST_ACTION_NONE;
}

static const char* action_enum_to_str(last_action_t action) {
    switch (action) {
        case ACTION_POWER_OFF:   return "POWER_OFF";
        case ACTION_POWER_ON:    return "POWER_ON";
        case ACTION_SET_TEMP_18: return "SET_TEMP_18";
        case ACTION_SET_TEMP_19: return "SET_TEMP_19";
        case ACTION_SET_TEMP_20: return "SET_TEMP_20";
        case ACTION_SET_TEMP_21: return "SET_TEMP_21";
        case ACTION_SET_TEMP_22: return "SET_TEMP_22";
        case ACTION_SET_TEMP_23: return "SET_TEMP_23";
        case ACTION_SET_TEMP_24: return "SET_TEMP_24";
        case ACTION_SET_TEMP_25: return "SET_TEMP_25";
        default:                 return "NONE";
    }
}

esp_err_t json_decode_schedule(const char *json_str, schedule_payload_t *out_payload) {
    if (json_str == NULL || out_payload == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) {
        ESP_LOGE(TAG, "Erro ao realizar parse do JSON no CMD 6");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *sched_id  = cJSON_GetObjectItemCaseSensitive(root, "schedule_id");
    cJSON *week_days = cJSON_GetObjectItemCaseSensitive(root, "week_days");
    cJSON *time_str  = cJSON_GetObjectItemCaseSensitive(root, "time");
    cJSON *action    = cJSON_GetObjectItemCaseSensitive(root, "action");

    if (!cJSON_IsNumber(sched_id) || !cJSON_IsNumber(week_days) ||
        !cJSON_IsString(time_str) || (time_str->valuestring == NULL) ||
        !cJSON_IsString(action)   || (action->valuestring == NULL)) {
        ESP_LOGE(TAG, "Campos inválidos ou ausentes no CMD 6");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    if (sched_id->valueint < 0 || sched_id->valueint >= MAX_SCHEDULE_ITEMS) {
        ESP_LOGE(TAG, "schedule_id fora dos limites permitidos (0-10): %d", sched_id->valueint);
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    memset(out_payload, 0, sizeof(schedule_payload_t));
    out_payload->schedule_id = (uint8_t)sched_id->valueint;
    out_payload->week_days   = (uint8_t)week_days->valueint;
    strncpy(out_payload->time, time_str->valuestring, SCHEDULE_TIME_STR_LEN - 1);
    out_payload->action      = action_str_to_enum(action->valuestring);

    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t json_encode_schedule_ack(const schedule_payload_t *payload, char *pub_buf, size_t max_len) {
    if (payload == NULL || pub_buf == NULL || max_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    int len = snprintf(pub_buf, max_len,
        "{\"cmd_id\":7,\"schedule_id\":%d,\"week_days\":%d,\"time\":\"%s\",\"action\":\"%s\"}",
        payload->schedule_id,
        payload->week_days,
        payload->time,
        action_enum_to_str(payload->action));

    if (len < 0 || (size_t)len >= max_len) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}