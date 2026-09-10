#include "board_spi_bus.h"
#include "esp_log.h"

static const char *TAG = "BOARD_SPI_BUS";
static bool s_bus_initialized = false;      /**< Flag to indicate if the SPI bus is initialized */

esp_err_t board_spi_bus_init(void) {
    if (s_bus_initialized) {
        return ESP_OK;
    }

    spi_bus_config_t buscfg = {
        .miso_io_num = BOARD_SPI_MISO_PIN,
        .mosi_io_num = BOARD_SPI_MOSI_PIN,
        .sclk_io_num = BOARD_SPI_SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };

    esp_err_t ret = spi_bus_initialize(BOARD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar SPI Bus: %s", esp_err_to_name(ret));
        return ret;
    }

    s_bus_initialized = true;
    ESP_LOGI(TAG, "Barramento SPI (CLK:%d, MISO:%d, MOSI:%d) inicializado!", 
             BOARD_SPI_SCLK_PIN, BOARD_SPI_MISO_PIN, BOARD_SPI_MOSI_PIN);
    return ESP_OK;
}

esp_err_t board_spi_bus_deinit(void) {
    if (!s_bus_initialized) return ESP_OK;
    
    esp_err_t ret = spi_bus_free(BOARD_SPI_HOST);
    if (ret == ESP_OK) {
        s_bus_initialized = false;
        ESP_LOGI(TAG, "Barramento SPI liberado.");
    }
    return ret;
}