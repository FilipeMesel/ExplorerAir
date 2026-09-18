#ifndef JSON_PROTOCOL_H
#define JSON_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "app_structs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SCHEDULES 11        /**< 0 - 10 Schedules */
#define MAX_IR_RAW_TIMINGS 700  /**< Maximum number of IR raw timings for CMD 7 */

/**
 * @brief Enumeration of Command IDs
 */
typedef enum {
    CMD_ID_TELEMETRY            = 0,
    CMD_ID_RTC_SYNC             = 1,
    CMD_ID_GET_IR_LEARNED       = 2,
    CMD_ID_IR_LEARNED           = 3,
    CMD_ID_WIFI_PROV            = 4,
    CMD_ID_WIFI_ACK             = 5,
    CMD_ID_SCHEDULE_PROV        = 6,
    CMD_ID_SCHEDULE_ACK         = 7,
    CMD_ID_SET_IR_RAW_DATA      = 8,
    CMD_ID_SET_IR_RAW_DATA_ACK  = 9,
} cmd_id_t;

/**
 * @brief Serializes telemetry data into JSON string (CMD 0).
 */
esp_err_t json_encode_telemetry(const telemetry_data_t *data, char *out_buf, size_t max_len);

/**
 * @brief Extract the cmd_id from the JSON string for command routing.
 */
esp_err_t json_get_cmd_id(const char *json_str, int *cmd_id);

/**
 * @brief Decodes CMD 1 JSON payload (Telemetry ACK & RTC Sync).
 */
esp_err_t json_decode_sync(const char *json_str, cmd1_sync_data_t *out_data);

/**
 * @brief Decodes the CMD 4 (Wi-Fi Provisioning) JSON payload.
 * 
 * @param json_str JSON string received via MQTT Downlink.
 * @param out_payload Pointer to the structure that will receive the SSID and password.
 * @return esp_err_t ESP_OK in the event of success.
 */
esp_err_t json_decode_wifi_prov(const char *json_str, wifi_prov_payload_t *out_payload);

/**
 * @brief Encodes the CMD 5 (Wi-Fi Received ACK) JSON payload.
 * 
 * @param payload Structure containing the SSID and password to be confirmed.
 * @param pub_buf Output buffer where the JSON string will be stored.
 * @param max_len Maximum output buffer size.
 * @return esp_err_t ESP_OK in the event of success.
 */
esp_err_t json_encode_wifi_ack(const wifi_prov_payload_t *payload, char *pub_buf, size_t max_len);

/**
 * @brief Decodes the CMD 6 (Schedule Provisioning) JSON payload.
 * 
 * @param json_str JSON string received via MQTT Downlink.
 * @param out_payload Pointer to the structure that will receive the decoded schedule.
 * @return esp_err_t ESP_OK in the event of success.
 */
esp_err_t json_decode_schedule(const char *json_str, schedule_payload_t *out_payload);

/**
 * @brief Encodes the CMD 7 JSON payload (Schedule ACK).
 * 
 * @param payload Structure containing the appointment data to be confirmed.
 * @param pub_buf Output buffer for the JSON string.
 * @param max_len Maximum output buffer size.
 * @return esp_err_t ESP_OK in the event of success.
 */
esp_err_t json_encode_schedule_ack(const schedule_payload_t *payload, char *pub_buf, size_t max_len);

/**
 * @brief Decodes the CMD 8 JSON payload (SET_IR_RAW_DATA).
 *
 * @param json_str Payload
 * @param out_action_idx action index
 * @param out_cmd ir struct object
 * @return esp_err_t ESP_OK in the event of success.
 */
esp_err_t json_decode_set_ir_raw(const char *json_str, uint8_t *out_action_idx, ir_raw_command_t *out_cmd);

/**
 * @brief Encodes the ACK response for CMD 9 (SET_IR_RAW_DATA_ACK) in JSON.
 *
 * @param action_idx action index
 * @param pub_buf payload
 * @param max_len payload size
 * @return esp_err_t ESP_OK in the event of success.
 */
esp_err_t json_encode_set_ir_raw_ack(uint8_t action_idx, char *pub_buf, size_t max_len);

/**
 * @brief Codifica o comando CMD 3 (IR Learned/Raw) contendo a ação, tamanho e os dados raw.
 * 
 * @param action_idx Índice da ação/slot (0 a 9).
 * @param cmd Ponteiro para a estrutura com os dados do comando IR lido da FRAM.
 * @param pub_buf Buffer de saída para o JSON.
 * @param max_len Tamanho máximo do buffer de saída.
 * @return esp_err_t ESP_OK em caso de sucesso.
 */
esp_err_t json_encode_cmd3_ir_raw(uint8_t action_idx, const ir_raw_command_t *cmd, char *pub_buf, size_t max_len);

/**
 * @brief Decodes CMD 2 JSON payload (GET_IR_LEARNED).
 */
esp_err_t json_decode_cmd2_get_ir(const char *json_str, uint8_t *out_requested_action);

#ifdef __cplusplus
}
#endif

#endif // JSON_PROTOCOL_H