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

#ifdef __cplusplus
}
#endif

#endif // FRAM_MB85RS512T_H