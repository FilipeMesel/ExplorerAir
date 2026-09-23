/**
 * @file board_spi_bus.h
 * @author Filipe Mesel Lobo Costa Cardoso
 * @brief 
 * @version 0.1
 * @date 2026-09-04
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#ifndef BOARD_SPI_BUS_H
#define BOARD_SPI_BUS_H

#include "esp_err.h"
#include "driver/spi_master.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BOARD_SPI_SCLK_PIN      15          /**< SCLK pin for SPI bus */
#define BOARD_SPI_MISO_PIN      34          /**< MISO pin for SPI bus */
#define BOARD_SPI_MOSI_PIN      2           /**< MOSI pin for SPI bus */
#define BOARD_SPI_FRAM_CS_PIN   16          /**< CS pin for FRAM */
#define BOARD_SPI_HOST          SPI2_HOST   /**< SPI host for the board */
/**
 * @brief Initialize the SPI master bus with the board's pins.
 */
esp_err_t board_spi_bus_init(void);

/**
 * @brief Deinitialize the SPI master bus.
 */
esp_err_t board_spi_bus_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // BOARD_SPI_BUS_H