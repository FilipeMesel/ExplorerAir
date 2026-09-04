/**
 * @file board_i2c_bus.h
 * @brief Thread-safe Shared I2C Bus Manager for ESP-IDF v6.0+
 * @author Filipe Mesel Lobo Costa Cardoso
 * @date 2026-09-03
 * 
 * @copyright Copyright (c) 2026
 */

#ifndef BOARD_I2C_BUS_H
#define BOARD_I2C_BUS_H

#include "esp_err.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BOARD_I2C_SDA_PIN       23          /**< Pin do SDA from I2C Bus */
#define BOARD_I2C_SCL_PIN       18          /**< Pin do SCL from I2C Bus */
#define BOARD_I2C_PORT_NUM      I2C_NUM_0   /**< Número do port from I2C Bus */

/**
 * @brief Initialize the I2C Master Bus shared and protection by mutex.
 * 
 * @note This function should be called only once during boot.
 * @return esp_err_t ESP_OK on success, or equivalent error code.
 */
esp_err_t board_i2c_bus_init(void);

/**
 * @brief Deinitialize the I2C Master Bus and free the mutex memory.
 * 
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t board_i2c_bus_deinit(void);

/**
 * @brief Get the handle of the initialized I2C Master Bus.
 * 
 * @return i2c_master_bus_handle_t Handle of the I2C Bus or NULL if not initialized.
 */
i2c_master_bus_handle_t board_i2c_bus_get_handle(void);

/**
 * @brief Get the exclusive lock of the I2C Bus (Thread-Safe Lock).
 * 
 * @param timeout_ms Timeout in milliseconds to acquire the lock.
 * @return esp_err_t ESP_OK if the lock was acquired, ESP_ERR_TIMEOUT otherwise.
 */
esp_err_t board_i2c_bus_lock(uint32_t timeout_ms);

/**
 * @brief Free the exclusive lock of the I2C Bus (Thread-Safe Unlock).
 * 
 * @return esp_err_t ESP_OK if the lock was freed successfully.
 */
esp_err_t board_i2c_bus_unlock(void);

/**
 * @brief Listen a scan on the I2C Bus and print the found addresses to the log.
 * 
 * @return esp_err_t ESP_OK after completing the scan.
 */
esp_err_t board_i2c_bus_scan(void);

#ifdef __cplusplus
}
#endif

#endif // BOARD_I2C_BUS_H