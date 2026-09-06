#ifndef FRAM_MB85RS512T_H
#define FRAM_MB85RS512T_H

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Command codes for the FRAM MB85RS512T device
#define FRAM_CMD_WREN   0x06  /**< Write Enable */
#define FRAM_CMD_WRDI   0x04  /**< Write Disable */
#define FRAM_CMD_RDSR   0x05  /**< Read Status Register */
#define FRAM_CMD_WRSR   0x01  /**< Write Status Register */
#define FRAM_CMD_READ   0x03  /**< Read Memory Code */
#define FRAM_CMD_WRITE  0x02  /**< Write Memory Code */
#define FRAM_CMD_RDID   0x9F  /**< Read Device ID */

// Offsets from the beginning of the FRAM memory space
#define FRAM_CONFIG_OFFSET       0x0000  /**< Length: 2 KB (0x0000 - 0x07FF) */
#define FRAM_SCHEDULES_OFFSET    0x0800  /**< Length: 4 KB (0x0800 - 0x17FF) */
#define FRAM_RING_BUFFER_OFFSET  0x1800  /**< Length: ~58 KB (0x1800 - 0xFFFF) */
#define FRAM_TOTAL_SIZE          0x10000 /**< 64 KB */

#define FRAM_RING_MAX_SLOTS      500                                /**< Maximum number of slots in the ring buffer */
#define FRAM_RING_MAGIC_HEADER   0x4652414D                         /**< Magic number: ASCII "FRAM" */
#define HEADER_OFFSET       0x0000                                  /**< Offset of the ring buffer header */
#define SLOTS_START_OFFSET  ((uint16_t)sizeof(fram_ring_header_t))  /**< Offset of the first slot in the ring buffer */

/**
 * @brief Compact telemetry log entry stored in each FRAM slot
 */
typedef struct __attribute__((packed)) {
    int16_t temperature;       /**< Temperature in Celsius */
    int16_t humidity;          /**< Relative humidity in % */
    uint8_t hour;              /**< RTC Hour (0-23) */
    uint8_t minute;            /**< RTC Minute (0-59) */
    int8_t rssi;               /**< Wi-Fi RSSI */
    uint16_t battery_mv;       /**< Battery voltage in millivolts */
    uint8_t last_action;       /**< Last action executed (last_action_t) */
    uint8_t reserved;          /**< Reserved byte for alignment */
} fram_log_entry_t;

/**
 * @brief Task 7.3.1: Ring Buffer Controller Header persisted at FRAM offset 0x0000
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;            /**< Validation Magic Number (0x4652414D) */
    uint16_t head;             /**< Write index for the next incoming log */
    uint16_t tail;             /**< Read index for the oldest log */
    uint16_t count;            /**< Current active item count (0 to 500) */
    uint16_t checksum;         /**< Simple XOR checksum for integrity check */
} fram_ring_header_t;

/**
 * @brief Initialize the FRAM device and add it to the SPI bus.
 */
esp_err_t fram_init(void);

/**
 * @brief Reads the Status Register (RDSR).
 */
esp_err_t fram_read_status(uint8_t *status);

/**
 * @brief Writes a buffer to the FRAM at a specific address (WRITE).
 */
esp_err_t fram_write(uint16_t address, const uint8_t *data, size_t len);

/**
 * @brief Reads a buffer from the FRAM starting at a specific address (READ).
 */
esp_err_t fram_read(uint16_t address, uint8_t *data, size_t len);

/* --- Functions from the abstract layer by offsets --- */

/**
 * @brief Writes configuration data to the FRAM.
 * 
 * @param data Pointer to the data to be written
 * @param len Length of the data to be written
 * @return esp_err_t 
 */
esp_err_t fram_write_config(const uint8_t *data, size_t len);

/**
 * @brief Reads configuration data from the FRAM.
 * 
 * @param data Pointer to the buffer where the data will be read
 * @param len Length of the data to be read
 * @return esp_err_t 
 */
esp_err_t fram_read_config(uint8_t *data, size_t len);

/**
 * @brief Writes schedule data to the FRAM.
 * 
 * @param data Pointer to the data to be written
 * @param len Length of the data to be written
 * @return esp_err_t 
 */
esp_err_t fram_write_schedules(const uint8_t *data, size_t len);

/**
 * @brief Reads schedule data from the FRAM.
 * 
 * @param data Pointer to the buffer where the data will be read
 * @param len Length of the data to be read
 * @return esp_err_t 
 */
esp_err_t fram_read_schedules(uint8_t *data, size_t len);

/**
 * @brief Writes telemetry data to the FRAM ring buffer.
 * 
 * @param relative_offset Relative offset within the ring buffer
 * @param data Pointer to the data to be written
 * @param len Length of the data to be written
 * @return esp_err_t 
 */
esp_err_t fram_write_telemetry_ring(uint16_t relative_offset, const uint8_t *data, size_t len);

/**
 * @brief Reads telemetry data from the FRAM ring buffer.
 * 
 * @param relative_offset Relative offset within the ring buffer
 * @param data Pointer to the buffer where the data will be read
 * @param len Length of the data to be read
 * @return esp_err_t 
 */
esp_err_t fram_read_telemetry_ring(uint16_t relative_offset, uint8_t *data, size_t len);
/**
 * @brief Initializes the Ring Buffer. Automatically formats if FRAM is uninitialized or corrupted.
 * 
 * @return esp_err_t ESP_OK on success, or underlying FRAM read/write error code.
 */
esp_err_t fram_ring_init(void);

/**
 * @brief Resets and clears the Ring Buffer by re-formatting the control header in FRAM.
 * 
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t fram_ring_clear(void);

/**
 * @brief Pushes a new telemetry entry to the FRAM ring buffer when offline.
 * 
 * @param[in] telemetry Pointer to the active telemetry data structure.
 * @return esp_err_t ESP_OK on success, or error code on failure.
 */
esp_err_t fram_ring_push(const fram_log_entry_t *entry);

/**
 * @brief Pops and removes the oldest pending telemetry entry from the FRAM ring buffer.
 * 
 * @param[out] out_telemetry Pointer to store the extracted telemetry data.
 * @return esp_err_t ESP_OK on success, ESP_ERR_NOT_FOUND if empty, or error code on failure.
 */
esp_err_t fram_ring_pop(fram_log_entry_t *out_entry);

/**
 * @brief Callback signature to handle sending a popped telemetry log over MQTT.
 * 
 * @param[in] telemetry Pointer to the popped telemetry data.
 * @return esp_err_t ESP_OK if sending succeeded, error code otherwise.
 */
typedef esp_err_t (*fram_flush_cb_t)(const fram_log_entry_t *entry);

/**
 * @brief Pops pending offline logs from FRAM and dispatches them via callback.
 * 
 * @param[in] pub_cb Function callback responsible for encoding and publishing the log.
 * @return esp_err_t ESP_OK if all logs were sent or queue is empty, or error code on failure.
 */
esp_err_t fram_ring_flush_to_mqtt(fram_flush_cb_t pub_cb);

/**
 * @brief Returns the total number of offline logs currently pending in FRAM.
 * 
 * @return uint16_t Pending log count.
 */
uint16_t fram_ring_get_count(void);

/**
 * @brief Executes self-diagnostic unit tests for the FRAM component.
 * 
 * @return esp_err_t ESP_OK if all tests pass, ESP_FAIL otherwise.
 */
esp_err_t fram_run_tests(void);

#ifdef __cplusplus
}
#endif

#endif // FRAM_MB85RS512T_H