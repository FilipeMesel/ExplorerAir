/**
 * @file json_protocol.h
 * @brief JSON Protocol Encoders for explorerAirConditioner (Uplink)
 * @author Embedded Systems Architect
 * @date 2026
 */

#ifndef JSON_PROTOCOL_H
#define JSON_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SCHEDULES 11  /**< 0 - 10 Schedules */

#define MAX_IR_RAW_TIMINGS 750 /**< Maximum number of IR raw timings for CMD 7 */

/**
 * @brief Enumeration of Last Actions performed before sleep/wake
 */
typedef enum {
    ACTION_TELEMETRY    = 0,
    ACTION_POWER_OFF    = 1,
    ACTION_POWER_ON     = 2,
    ACTION_SET_TEMP_18  = 3,
    ACTION_SET_TEMP_19  = 4,
    ACTION_SET_TEMP_20  = 5,
    ACTION_SET_TEMP_21  = 6,
    ACTION_SET_TEMP_22  = 7,
    ACTION_SET_TEMP_23  = 8,
    ACTION_SET_TEMP_24  = 9,
    ACTION_SET_TEMP_25  = 10
} last_action_t;

/**
 * @brief Enumeration of Command IDs
 */
enum {
    CMD_ID_TELEMETRY = 0,
    CMD_ID_RTC_SYNC,
    CMD_ID_IR_LEARNED,
    CMD_ID_WIFI_PROV,
    CMD_ID_WIFI_ACK,
    CMD_ID_SCHEDULE_PROV,
    CMD_ID_SCHEDULE_ACK,
    CMD_ID_SET_IR_RAW_DATA,
    CMD_ID_SET_IR_RAW_DATA_ACK,
};

/**
 * @brief Telemetry payload structure for CMD 0
 */
typedef struct {
    int temperature;           /**< Temperature reading in Celsius */
    int humidity;              /**< Humidity reading in % */
    char rtc_time[6];          /**< Formatted time string "HH:MM" (null-terminated) */
    char rssi[8];              /**< RSSI string, e.g., "-10" */
    float battery_voltage;     /**< Battery voltage in Volts (e.g., 3.7) */
    last_action_t last_action; /**< Last action performed */
} telemetry_data_t;

/**
 * @brief Wi-Fi Ack payload structure for CMD 4
 */
typedef struct {
    const char *ssid;     /**< SSID received from server */
    const char *password; /**< Password received from server */
} wifi_ack_payload_t;

/**
 * @brief Schedule Ack payload structure for CMD 6
 */
typedef struct {
    uint8_t week_days;    /**< Weekdays bitmask (0-127) */
    const char *time;     /**< Schedule time "HH:MM" */
    const char *action;   /**< Action string (e.g., "SET_TEMP_18") */
    const char *status;   /**< Status response, typically "OK" */
} schedule_ack_payload_t;

/**
 * @brief IR Raw Data payload structure for CMD 7
 */
typedef struct {
    uint16_t frequency_hz;                  /**< Frequência da portadora em Hz (ex: 38000) */
    uint16_t timings[MAX_IR_RAW_TIMINGS];   /**< Vetor com os tempos em microsegundos */
    uint16_t timings_count;                 /**< Quantidade de tempos recebidos */
} cmd7_ir_raw_payload_t;

// --- CMD 8 Payload Structure (Set IR Raw Data ACK) ---
/**
 * @brief IR Raw Data ACK payload structure for CMD 8
 */
typedef struct {
    const char *status;                     /**< Processing Status, eg: "OK" or "ERROR" */
    uint16_t count_received;                /**< Processed Quantity of Valid Timings */
} cmd8_ir_raw_ack_payload_t;

// --- CMD 1 Payload Structure ---
/**
 * @brief RTC Sync payload structure for CMD 1 
 */
typedef struct {
    uint8_t day;
    uint8_t month;
    uint16_t year;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint16_t interval_sec;
} cmd1_rtc_sync_payload_t;

// --- CMD 3 Payload Structure ---
/**
 * @brief Wi-Fi Provisioning payload structure for CMD 3
 */
typedef struct {
    char ssid[33];        /**< Buffer for SSID (max 32 chars + null) */
    char password[65];    /**< Buffer for password (max 64 chars + null) */
} cmd3_wifi_prov_payload_t;

/**
 * @brief Schedule item structure
 */
typedef struct {
    uint8_t index;        /**< Index of the schedule (0 to 10) */
    uint8_t week_days;    /**< Bitmask of weekdays (0 to 127) */
    char time[6];         /**< Formatted time "HH:MM" */
    char action[20];      /**< Associated action (e.g., "POWER_ON", "SET_TEMP_22") */
} schedule_item_t;

/**
 * @brief Schedule payload structure for CMD 5
 */
typedef struct {
    schedule_item_t items[MAX_SCHEDULES];       /**< Array of schedule items */
    uint8_t count;                              /**< Quantity of schedules parsed in the array */
} cmd5_schedule_payload_t;

/**
 * @brief Serializes CMD 0 (Initial Telemetry) into a JSON string.
 * 
 * @param[in]  data    Pointer to telemetry data structure.
 * @param[out] out_str Pointer to char* where allocated JSON string will be stored.
 * 
 * @note The caller MUST free() the returned pointer (*out_str) after use.
 * 
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NO_MEM on failure.
 */
esp_err_t json_encode_telemetry(const telemetry_data_t *data, char **out_str);

/**
 * @brief Serializes CMD 4 (Wi-Fi Received ACK) into a JSON string.
 * 
 * @param[in]  ack     Pointer to Wi-Fi ACK payload structure.
 * @param[out] out_str Pointer to char* where allocated JSON string will be stored.
 * 
 * @note The caller MUST free() the returned pointer (*out_str) after use.
 * 
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NO_MEM on failure.
 */
esp_err_t json_encode_wifi_ack(const wifi_ack_payload_t *ack, char **out_str);

/**
 * @brief Serializes CMD 6 (Schedule ACK) into a JSON string.
 * 
 * @param[in]  ack     Pointer to Schedule ACK payload structure.
 * @param[out] out_str Pointer to char* where allocated JSON string will be stored.
 * 
 * @note The caller MUST free() the returned pointer (*out_str) after use.
 * 
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NO_MEM on failure.
 */
esp_err_t json_encode_schedule_ack(const schedule_ack_payload_t *ack, char **out_str);

/**
 * @brief Build and serialize CMD 0 (Telemetry) JSON string directly from raw parameters.
 * 
 * @param[in] temperature     Temperature reading in Celsius.
 * @param[in] humidity        Humidity reading in %.
 * @param[in] rtc_time        Formatted time string "HH:MM" (e.g., "10:00").
 * @param[in] rssi            RSSI string (e.g., "-10").
 * @param[in] battery_voltage Battery voltage in Volts (e.g., 3.7f).
 * @param[in] last_action     Last action executed (e.g., ACTION_POWER_ON).
 * 
 * @note The caller MUST free() the returned pointer after use to prevent memory leaks.
 * 
 * @return Allocated JSON string pointer on success, or NULL if encoding fails.
 */
char* build_telemetry_json(int temperature,  int humidity, 
                           const char *rtc_time,  const char *rssi, 
                           float battery_voltage,  last_action_t last_action);

/**
 * @brief Extract the cmd_id from the JSON string for command routing.
 * 
 * @param[in]  json_str Json String JSON received by the MQTT system.
 * @param[out] cmd_id   Pointer where the command ID will be stored.
 * 
 * @return ESP_OK if the cmd_id is found, ESP_ERR_INVALID_ARG otherwise.
 */
esp_err_t json_get_cmd_id(const char *json_str, int *cmd_id);

/**
 * @brief CMD 1 Parser (RTC Timer Sync + Telemetry Interval).
 */
esp_err_t json_decode_cmd1_rtc_sync(const char *json_str, cmd1_rtc_sync_payload_t *payload);

/**
 * @brief CMD 3 Parser (Wi-Fi Credentials Provisioning).
 */
esp_err_t json_decode_cmd3_wifi_prov(const char *json_str, cmd3_wifi_prov_payload_t *payload);

/**
 * @brief CMD 5 Parser (Schedule Provisioning).
 */
esp_err_t json_decode_cmd5_schedule(const char *json_str, cmd5_schedule_payload_t *payload);

/**
 * @brief Decode the JSON payload of CMD 7 (Set IR Raw Data).
 * 
 * @param[in]  json_str Joson String received from MQTT.
 * @param[out] payload  Pointer to the structure where the extracted data will be stored.
 * 
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on failure.
 */
esp_err_t json_decode_cmd7_ir_raw(const char *json_str, cmd7_ir_raw_payload_t *payload);

/**
 * @brief Serializes the ACK response of CMD 8 (Set IR Raw Data ACK) to JSON.
 * 
 * @param[in]  ack     Pointer to the ACK data structure of CMD 8.
 * @param[out] out_str Pointer to char* where the allocated JSON string will be stored.
 * 
 * @note The caller must free the pointer (*out_str) using free() after use.
 * 
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG or ESP_ERR_NO_MEM on failure.
 */
esp_err_t json_encode_ir_raw_ack(const cmd8_ir_raw_ack_payload_t *ack, char **out_str);

#ifdef __cplusplus
}
#endif

#endif // JSON_PROTOCOL_H