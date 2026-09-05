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

#ifdef __cplusplus
}
#endif

#endif // JSON_PROTOCOL_H