#ifndef APP_STORAGE_H
#define APP_STORAGE_H

#include "esp_err.h"
#include "app_structs.h"

/* =========================================================================
 * FRAM MEMORY MAP (MB85RS512T - 64 KB Total / 0x0000 to 0xFFFF)
 * ========================================================================= */
#define FRAM_ADDR_SYS_CONFIG        0x0000 /**< System settings (Telemetry interval, etc.) */
#define FRAM_ADDR_WIFI_CREDENTIALS  0x0020 /**< Location of client Wi-Fi credentials (97 bytes) */
#define FRAM_ADDR_SCHEDULE_TABLE    0x0100 /**< Schedule table (11 * sizeof(schedule_payload_t)) */
#define FRAM_ADDR_WAKEUP_CONTEXT    0x0200 /**< Context of the next wakeup */
#define FRAM_ADDR_RING_BUFFER_LOGS  0x0300 /**< Offline Telemetry FIFO Queue / Ring Buffer */

/**
 * @brief Initializes the storage module and the FRAM driver.
 * @return ESP_OK if the SPI driver and the FRAM respond correctly.
 */
esp_err_t app_storage_init(void);

/**
 * @brief Updates the telemetry interval in the FRAM.
 * @param interval_sec New interval in seconds.
 * @return ESP_OK in the event of success.
 */
esp_err_t app_storage_save_telemetry_interval(uint16_t interval_sec);

/**
 * @brief Reads the telemetry interval saved in FRAM.
 * @param interval_sec Pointer to store the read value.
 * @return ESP_OK in the event of success.
 */
esp_err_t app_storage_get_telemetry_interval(uint16_t *interval_sec);

/**
 * @brief Saves dynamic Wi-Fi credentials to FRAM.
 * @param creds Pointer to the structure containing the SSID and password.
 * @return ESP_OK in the event of success.
 */
esp_err_t app_storage_save_wifi_credentials(const wifi_credentials_t *creds);

/**
 * @brief Reads the dynamic Wi-Fi credentials saved in FRAM.
 * @param creds Pointer where the credentials will be loaded.
 * @return ESP_OK if loaded and valid; ESP_ERR_NOT_FOUND if no credential is saved.
 */
esp_err_t app_storage_get_wifi_credentials(wifi_credentials_t *creds);

/**
 * @brief Saves or updates a specific schedule at the schedule_id index in the FRAM.
 * @param schedule Pointer to the structure containing the scheduling data.
 * @return ESP_OK  in the event of success.
 */
esp_err_t app_storage_save_schedule(const schedule_payload_t *schedule);

/**
 * @brief Reads a specific schedule stored in FRAM by its ID (0 to 10).
 * @param schedule_id ID of the schedule to be read.
 * @param out_schedule Pointer where the data will be stored.
 * @return ESP_OK  in the event of success.
 */
esp_err_t app_storage_get_schedule(uint8_t schedule_id, schedule_payload_t *out_schedule);

/**
 * @brief The context of the upcoming awakening event at FRAM persists.
 * @param ctx Pointer to the wakeup context structure.
 * @return ESP_OK  in the event of success.
 */
esp_err_t app_storage_save_wakeup_context(const wakeup_context_t *ctx);

/**
 * @brief Retrieves the wakeup context stored in FRAM.
 * @param ctx Pointer for data entry.
 * @return ESP_OK in the event of success.
 */
esp_err_t app_storage_get_wakeup_context(wakeup_context_t *ctx);

/**
 * @brief Inserts a telemetry_data_t structure into the FIFO queue in FRAM.
 *        If it reaches 100 records, the oldest record is overwritten.
 */
esp_err_t app_storage_push_telemetry_log(const telemetry_data_t *log_entry);

/**
 * @brief Removes and returns the oldest telemetry_data_t from the FIFO queue.
 */
esp_err_t app_storage_pop_telemetry_log(telemetry_data_t *out_entry);

/**
 * @brief Retrieves the number of pending records in the FRAM.
 */
esp_err_t app_storage_get_telemetry_log_count(uint16_t *out_count);

/**
 * @brief Resets the FIFO queue in FRAM.
 */
esp_err_t app_storage_clear_telemetry_queue(void);

#endif // APP_STORAGE_H