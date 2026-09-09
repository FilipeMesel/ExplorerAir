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