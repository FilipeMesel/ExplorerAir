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
#define MAX_IR_RAW_TIMINGS 750  /**< Maximum number of IR raw timings for CMD 7 */

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
 * @brief Decodifica o payload JSON do CMD 4 (Wi-Fi Provisioning).
 * 
 * @param json_str String JSON recebida via MQTT Downlink.
 * @param out_payload Ponteiro para a estrutura que receberá o SSID e Password.
 * @return esp_err_t ESP_OK em caso de sucesso.
 */
esp_err_t json_decode_wifi_prov(const char *json_str, wifi_prov_payload_t *out_payload);

/**
 * @brief Codifica o payload JSON do CMD 5 (Wi-Fi Received ACK).
 * 
 * @param payload Estrutura contendo o SSID e Password a serem confirmados.
 * @pub_buf Buffer de saída onde a string JSON será armazenada.
 * @max_len Tamanho máximo do buffer de saída.
 * @return esp_err_t ESP_OK em caso de sucesso.
 */
esp_err_t json_encode_wifi_ack(const wifi_prov_payload_t *payload, char *pub_buf, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif // JSON_PROTOCOL_H