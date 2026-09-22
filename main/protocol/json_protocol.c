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

    // Read JSON's fields
    cJSON *hour             = cJSON_GetObjectItemCaseSensitive(root, "hour");
    cJSON *minute           = cJSON_GetObjectItemCaseSensitive(root, "min");
    cJSON *second           = cJSON_GetObjectItemCaseSensitive(root, "sec");
    cJSON *weekday          = cJSON_GetObjectItemCaseSensitive(root, "weekday");
    cJSON *day              = cJSON_GetObjectItemCaseSensitive(root, "day");
    cJSON *month            = cJSON_GetObjectItemCaseSensitive(root, "month");
    cJSON *year             = cJSON_GetObjectItemCaseSensitive(root, "year");
    cJSON *telemetry_update = cJSON_GetObjectItemCaseSensitive(root, "telemetry_update");

    // Field's validation
    if (!cJSON_IsNumber(hour) || !cJSON_IsNumber(minute) || !cJSON_IsNumber(second)) {
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    // Entering the time and day of the week
    out_data->sync_time_t.hour    = (uint8_t)hour->valueint;
    out_data->sync_time_t.minute  = (uint8_t)minute->valueint;
    out_data->sync_time_t.second  = (uint8_t)second->valueint;
    out_data->sync_time_t.weekday = cJSON_IsNumber(weekday) ? (uint8_t)weekday->valueint : 0;

    // Date filling (with fallback to 0 if missing)
    out_data->sync_time_t.day     = cJSON_IsNumber(day)   ? (uint8_t)day->valueint   : 0;
    out_data->sync_time_t.month   = cJSON_IsNumber(month) ? (uint8_t)month->valueint : 0;
    out_data->sync_time_t.year    = cJSON_IsNumber(year)  ? (uint16_t)year->valueint : 0;

    // Decoding the new field: telemetry_update (in seconds)
    if (cJSON_IsNumber(telemetry_update)) {
        out_data->telemetry_update = telemetry_update->valueint;
    } else {
        // Default security value (e.g., 300s = 5 min) if the field is missing from the JSON.
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
        "{\"cmd_id\":%d,\"ssid\":\"%s\",\"password\":\"%s\"}",
        CMD_ID_WIFI_ACK,
        payload->ssid,
        payload->password);

    if (len < 0 || (size_t)len >= max_len) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

static ir_action_slot_t action_str_to_enum(const char *str) {
    if (!str) return IR_ACTION_NONE;
    if (strcmp(str, "POWER_OFF") == 0)   return IR_ACTION_POWER_OFF;
    if (strcmp(str, "POWER_ON") == 0)    return IR_ACTION_POWER_ON;
    if (strcmp(str, "SET_TEMP_18") == 0) return IR_ACTION_SET_TEMP_18;
    if (strcmp(str, "SET_TEMP_19") == 0) return IR_ACTION_SET_TEMP_19;
    if (strcmp(str, "SET_TEMP_20") == 0) return IR_ACTION_SET_TEMP_20;
    if (strcmp(str, "SET_TEMP_21") == 0) return IR_ACTION_SET_TEMP_21;
    if (strcmp(str, "SET_TEMP_22") == 0) return IR_ACTION_SET_TEMP_22;
    if (strcmp(str, "SET_TEMP_23") == 0) return IR_ACTION_SET_TEMP_23;
    if (strcmp(str, "SET_TEMP_24") == 0) return IR_ACTION_SET_TEMP_24;
    if (strcmp(str, "SET_TEMP_25") == 0) return IR_ACTION_SET_TEMP_25;
    return IR_ACTION_NONE;
}

static const char* action_enum_to_str(ir_action_slot_t action) {
    switch (action) {
        case IR_ACTION_POWER_OFF:   return "POWER_OFF";
        case IR_ACTION_POWER_ON:    return "POWER_ON";
        case IR_ACTION_SET_TEMP_18: return "SET_TEMP_18";
        case IR_ACTION_SET_TEMP_19: return "SET_TEMP_19";
        case IR_ACTION_SET_TEMP_20: return "SET_TEMP_20";
        case IR_ACTION_SET_TEMP_21: return "SET_TEMP_21";
        case IR_ACTION_SET_TEMP_22: return "SET_TEMP_22";
        case IR_ACTION_SET_TEMP_23: return "SET_TEMP_23";
        case IR_ACTION_SET_TEMP_24: return "SET_TEMP_24";
        case IR_ACTION_SET_TEMP_25: return "SET_TEMP_25";
        default:                    return "NONE";
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
        "{\"cmd_id\":%d,\"schedule_id\":%d,\"week_days\":%d,\"time\":\"%s\",\"action\":\"%s\"}",
        CMD_ID_SCHEDULE_ACK,
        payload->schedule_id,
        payload->week_days,
        payload->time,
        action_enum_to_str(payload->action));

    if (len < 0 || (size_t)len >= max_len) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t json_decode_set_ir_raw(const char *json_str, uint8_t *out_action_idx, ir_raw_command_t *out_cmd) {
    if (json_str == NULL || out_action_idx == NULL || out_cmd == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) {
        ESP_LOGE(TAG, "Erro ao realizar parse do JSON no CMD 8");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *action_item = cJSON_GetObjectItemCaseSensitive(root, "action");
    cJSON *raw_item = cJSON_GetObjectItemCaseSensitive(root, "raw_data");

    if (!cJSON_IsNumber(action_item) || !cJSON_IsArray(raw_item)) {
        ESP_LOGE(TAG, "Campos 'action_idx' ou 'raw_data' ausentes ou inválidos no CMD 8");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t action_idx = (uint8_t)action_item->valueint;
    if (action_idx >= IR_SLOT_COUNT) {
        ESP_LOGE(TAG, "action_idx fora dos limites: %d", action_idx);
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    int raw_size = cJSON_GetArraySize(raw_item);
    if (raw_size <= 0 || raw_size > MAX_IR_BUFFER_SIZE) {
        ESP_LOGE(TAG, "Tamanho de dados IR RAW inválido (%d)", raw_size);
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    out_cmd->length = (uint16_t)raw_size;
    for (int i = 0; i < raw_size; i++) {
        cJSON *elem = cJSON_GetArrayItem(raw_item, i);
        if (cJSON_IsNumber(elem)) {
            out_cmd->data[i] = (uint16_t)elem->valueint;
        } else {
            out_cmd->data[i] = 0;
        }
    }

    *out_action_idx = action_idx;
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t json_encode_set_ir_raw_ack(uint8_t action_idx, char *pub_buf, size_t max_len) {
    if (pub_buf == NULL || max_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    int len = snprintf(pub_buf, max_len,
        "{\"cmd_id\":%d,\"action\":%d,\"status\":\"SUCCESS\"}",
        CMD_ID_SET_IR_RAW_DATA_ACK,
        action_idx);

    if (len < 0 || (size_t)len >= max_len) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t json_encode_cmd3_ir_raw(uint8_t action_idx, const ir_raw_command_t *cmd, char *pub_buf, size_t max_len) {
    if (cmd == NULL || pub_buf == NULL || max_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return ESP_ERR_NO_MEM;
    }

    // Get static fields
    cJSON_AddNumberToObject(root, "cmd_id", CMD_ID_IR_LEARNED);
    cJSON_AddNumberToObject(root, "action", action_idx);
    cJSON_AddNumberToObject(root, "length", cmd->length);

    // Create the raw_data buffer
    cJSON *raw_array = cJSON_CreateArray();
    if (raw_array == NULL) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }

    for (int i = 0; i < cmd->length; i++) {
        cJSON_AddItemToArray(raw_array, cJSON_CreateNumber(cmd->data[i]));
    }
    cJSON_AddItemToObject(root, "raw_data", raw_array);

    // Create the Json String
    char *rendered = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (rendered == NULL) {
        return ESP_ERR_NO_MEM;
    }

    if (strlen(rendered) >= max_len) {
        free(rendered);
        return ESP_ERR_NO_MEM;
    }

    strncpy(pub_buf, rendered, max_len - 1);
    pub_buf[max_len - 1] = '\0';
    free(rendered);

    return ESP_OK;
}

esp_err_t json_decode_cmd2_get_ir(const char *json_str, uint8_t *out_requested_action) {
    if (json_str == NULL || out_requested_action == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *action_item = cJSON_GetObjectItemCaseSensitive(root, "action");
    if (!cJSON_IsNumber(action_item)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    *out_requested_action = (uint8_t)action_item->valueint;
    cJSON_Delete(root);
    return ESP_OK;
}