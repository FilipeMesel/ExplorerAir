/**
 * @file explorer_memory.c
 * @author Filipe Mesel Lobo Costa Cardoso
 * @brief Implementation of memory management functions for the Explorer IR Blaster project, including saving and loading IR commands, WiFi credentials, and schedules.
 * @version 0.1
 * @date 2026-08-31
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include "explorer_memory.h"
#include "esp_log.h"
#include "config.h"
#include "explorer_structs.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "EXPLORER_MEM";

bool explorer_memory_init(void) {
    esp_err_t ret = ESP_OK;
#ifdef _INCLUDE_NVS_
    nvs_flash_init();
    if (ret == ESP_ERR_NVS_NEW_VERSION_FOUND || ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
#endif
    return (ret == ESP_OK);
}

bool explorer_memory_save_ir(int cmd_index, const ir_raw_command_t *cmd) {
    if (cmd == NULL) return false;

#ifdef _INCLUDE_NVS_
    nvs_handle_t nvs_h;
    esp_err_t err = nvs_open(NVS_IR_NAMESPACE, NVS_READWRITE, &nvs_h);
    if (err != ESP_OK) return false;

    char key[16];
    snprintf(key, sizeof(key), "cmd_%d", cmd_index);

    err = nvs_set_blob(nvs_h, key, cmd, sizeof(ir_raw_command_t));
    if (err == ESP_OK) {
        err = nvs_commit(nvs_h);
    }
    nvs_close(nvs_h);
#endif
    return (err == ESP_OK);
}

bool explorer_memory_load_ir(int cmd_index, ir_raw_command_t *cmd_out) {
    if (cmd_out == NULL) return false;

#ifdef _INCLUDE_NVS_
    nvs_handle_t nvs_h;
    esp_err_t err = nvs_open(NVS_IR_NAMESPACE, NVS_READONLY, &nvs_h);
    if (err != ESP_OK) return false;

    char key[16];
    snprintf(key, sizeof(key), "cmd_%d", cmd_index);

    size_t required_size = sizeof(ir_raw_command_t);
    err = nvs_get_blob(nvs_h, key, cmd_out, &required_size);
    nvs_close(nvs_h);
#endif
    return (err == ESP_OK);
}

bool explorer_memory_save_wifi_credentials(const char *ssid, const char *password) {
#ifdef _INCLUDE_NVS_
    nvs_handle_t nvs_h;
    esp_err_t err = nvs_open(NVS_WIFI_NAMESPACE, NVS_READWRITE, &nvs_h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao abrir NVS para Wi-Fi: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_set_str(nvs_h, NVS_KEY_SSID, ssid);
    if (err == ESP_OK) {
        err = nvs_set_str(nvs_h, NVS_KEY_PASS, password);
    }

    if (err == ESP_OK) {
        err = nvs_commit(nvs_h);
        ESP_LOGI(TAG, "Novas credenciais Wi-Fi salvas com sucesso!");
    } else {
        ESP_LOGE(TAG, "Erro ao salvar credenciais na NVS: %s", esp_err_to_name(err));
    }

    nvs_close(nvs_h);
    return (err == ESP_OK);
#else
    return false;
#endif
}

bool explorer_memory_load_wifi_credentials(char *ssid_out, size_t max_ssid_len, char *pass_out, size_t max_pass_len) {
#ifdef _INCLUDE_NVS_
    nvs_handle_t nvs_h;
    esp_err_t err = nvs_open(NVS_WIFI_NAMESPACE, NVS_READONLY, &nvs_h);
    if (err != ESP_OK) return false;

    err = nvs_get_str(nvs_h, NVS_KEY_SSID, ssid_out, &max_ssid_len);
    if (err == ESP_OK) {
        err = nvs_get_str(nvs_h, NVS_KEY_PASS, pass_out, &max_pass_len);
    }

    nvs_close(nvs_h);
    return (err == ESP_OK);
#else
    return false;
#endif
}

bool explorer_memory_save_schedule(const schedule_t *sched) {
    if (sched == NULL || sched->schedule_id >= MAX_SCHEDULES) {
        ESP_LOGE(TAG, "Agendamento inválido.");
        return false;
    }

#ifdef _INCLUDE_NVS_
    nvs_handle_t nvs_h;
    esp_err_t err = nvs_open(NVS_SCHEDULE_NAMESPACE, NVS_READWRITE, &nvs_h);
    if (err != ESP_OK) return false;

    char key[16];
    snprintf(key, sizeof(key), "sched_%d", sched->schedule_id);

    err = nvs_set_blob(nvs_h, key, sched, sizeof(schedule_t));
    if (err == ESP_OK) {
        err = nvs_commit(nvs_h);
    }

    nvs_close(nvs_h);
    return (err == ESP_OK);
#else
    return false;
#endif
}

bool explorer_memory_load_schedule(uint8_t schedule_id, schedule_t *sched_out) {
    if (schedule_id >= MAX_SCHEDULES || sched_out == NULL) return false;

#ifdef _INCLUDE_NVS_
    nvs_handle_t nvs_h;
    esp_err_t err = nvs_open(NVS_SCHEDULE_NAMESPACE, NVS_READONLY, &nvs_h);
    if (err != ESP_OK) return false;

    char key[16];
    snprintf(key, sizeof(key), "sched_%d", schedule_id);

    size_t required_size = sizeof(schedule_t);
    err = nvs_get_blob(nvs_h, key, sched_out, &required_size);
    nvs_close(nvs_h);

    return (err == ESP_OK);
#else
    return false;
#endif
}

/**
 * @brief Gets the index of an action based on its string representation
 *
 * @param action_str The string representation of the action
 * @return int The index of the action, or -1 if not found
 */
int explorer_memory_get_index_by_action(const char *action_str) {
    if (action_str == NULL) return UNKNOWN_ACTION;

    // Get the index based on the action string. This mapping should correspond to the order of actions defined in IR_ACTIONS.
    if (strcmp(action_str, "LIGAR") == 0) return LIGAR;
    if (strcmp(action_str, "DESLIGAR") == 0) return DESLIGAR;
    if (strcmp(action_str, "18 C") == 0) return TEMP_18;
    if (strcmp(action_str, "19 C") == 0) return TEMP_19;
    if (strcmp(action_str, "20 C") == 0) return TEMP_20;
    if (strcmp(action_str, "21 C") == 0) return TEMP_21;
    if (strcmp(action_str, "22 C") == 0) return TEMP_22;
    if (strcmp(action_str, "23 C") == 0) return TEMP_23;
    if (strcmp(action_str, "24 C") == 0) return TEMP_24;
    if (strcmp(action_str, "25 C") == 0) return TEMP_25;

    return UNKNOWN_ACTION;
}