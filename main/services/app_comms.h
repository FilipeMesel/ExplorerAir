#ifndef APP_COMMS_H
#define APP_COMMS_H

#include "esp_err.h"
#include "app_events.h"
#include "app_structs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MQTT_SEND_INITIAL_TELEMETRY_BUFFER_LEN  300 /**< Send initial telemetry buffer length */
#define MQTT_ACK_BUFFER_LEN                     256 /**< ACK buffer length */

/**
 * @brief Initializes the communication stack (Wi-Fi and MQTT) and registers event callbacks.
 * @return ESP_OK on success.
 */
esp_err_t app_comms_init(void);

/**
 * @brief Initiates the Wi-Fi connection sequence with failover support.
 * @return ESP_OK on success.
 */
esp_err_t app_comms_wifi_start_failover(void);

/**
 * @brief Sends simulated initial telemetry via MQTT and posts the status to the global queue.
 * @return ESP_OK on success.
 */
esp_err_t app_comms_send_initial_telemetry(void);

/**
 * @brief Processes and routes JSON commands received via MQTT.
 * @param json_str JSON string received on the MQTT topic.
 * @return ESP_OK on success.
 */
esp_err_t app_comms_process_mqtt_command(const char *json_str);

/**
 * @brief Handler for Wi-Fi stack events. Posts events to g_app_event_queue.
 */
void app_comms_on_wifi_event(void *handler_args, esp_event_base_t base, int32_t id, void *data);

/**
 * @brief Handler for MQTT client events. Posts events to g_app_event_queue.
 */
void app_comms_on_mqtt_event(void *handler_args, esp_event_base_t base, int32_t id, void *data);

esp_err_t app_comms_get_wifi_credentials_from_fram(void);

/**
 * @brief Publishes the confirmation message (CMD 9) specifically for the Download index (idx = 255).
 * @return ESP_OK in case of success
 */
esp_err_t app_comms_send_ir_download_ack(void);

/**
 * @brief Sends CMD 3, signaling the execution of the IR shutdown command.
 * @return ESP_OK in case of success
 */
esp_err_t app_comms_send_ir_power_off_cmd(void);

/**
 * @brief Executes the sequential IR transmission sequence (Actions 0 to 9) with control
 *        of retries (3x per action) and waiting for CMD 2 (platform ACK).
 * @return ESP_OK if the entire sequence completed successfully; ESP_FAIL if any slot fails 3 times.
 */
esp_err_t app_comms_run_ir_sequence(void);

#ifdef __cplusplus
}
#endif

#endif // APP_COMMS_H