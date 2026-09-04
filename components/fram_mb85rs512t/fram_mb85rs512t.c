#include "fram_mb85rs512t.h"
#include "board_spi_bus.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "driver/spi_master.h"
#include <string.h>

static const char *TAG = "FRAM_MB85RS512T";
static spi_device_handle_t s_fram_spi_handle = NULL;    /**< SPI device handle for the FRAM */

static esp_err_t fram_send_wren(void) {
    spi_transaction_t t = {
        .length = 8,
        .flags = SPI_TRANS_USE_TXDATA,
        .tx_data[0] = FRAM_CMD_WREN,
    };
    return spi_device_polling_transmit(s_fram_spi_handle, &t);
}

esp_err_t fram_init(void) {
    esp_err_t ret = board_spi_bus_init();
    if (ret != ESP_OK) return ret;

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 10 * 1000 * 1000, // 10 MHz
        .mode = 0,                           // SPI Mode 0 (CPOL=0, CPHA=0)
        .spics_io_num = BOARD_SPI_FRAM_CS_PIN,
        .queue_size = 7,
    };

    ret = spi_bus_add_device(BOARD_SPI_HOST, &devcfg, &s_fram_spi_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Dispositivo FRAM registrado na SPI (CS: GPIO %d)", BOARD_SPI_FRAM_CS_PIN);
    }
    return ret;
}

esp_err_t fram_read_status(uint8_t *status) {
    if (!status || !s_fram_spi_handle) return ESP_ERR_INVALID_ARG;

    spi_transaction_t t = {
        .length = 16,
        .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
        .tx_data[0] = FRAM_CMD_RDSR,
    };

    esp_err_t ret = spi_device_polling_transmit(s_fram_spi_handle, &t);
    if (ret == ESP_OK) {
        *status = t.rx_data[1];
    }
    return ret;
}

esp_err_t fram_write(uint16_t address, const uint8_t *data, size_t len) {
    if (!data || len == 0 || (address + len) > FRAM_TOTAL_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    // Send the command WREM before writing
    esp_err_t ret = fram_send_wren();
    if (ret != ESP_OK) return ret;

    uint8_t cmd_header[3] = {
        FRAM_CMD_WRITE,
        (uint8_t)(address >> 8),
        (uint8_t)(address & 0xFF)
    };

    spi_transaction_t t = {
        .length = (3 + len) * 8,
        .flags = 0,
    };

    uint8_t *tx_buf = heap_caps_malloc(3 + len, MALLOC_CAP_DMA);
    if (!tx_buf) return ESP_ERR_NO_MEM;

    memcpy(tx_buf, cmd_header, 3);
    memcpy(tx_buf + 3, data, len);

    t.tx_buffer = tx_buf;
    ret = spi_device_polling_transmit(s_fram_spi_handle, &t);

    free(tx_buf);
    return ret;
}

esp_err_t fram_read(uint16_t address, uint8_t *data, size_t len) {
    if (!data || len == 0 || (address + len) > FRAM_TOTAL_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t cmd_header[3] = {
        FRAM_CMD_READ,
        (uint8_t)(address >> 8),
        (uint8_t)(address & 0xFF)
    };

    spi_transaction_t t = {
        .length = (3 + len) * 8,
        .tx_buffer = heap_caps_malloc(3 + len, MALLOC_CAP_DMA),
        .rx_buffer = heap_caps_malloc(3 + len, MALLOC_CAP_DMA),
    };

    if (!t.tx_buffer || !t.rx_buffer) {
        if (t.tx_buffer) free((void*)t.tx_buffer);
        if (t.rx_buffer) free(t.rx_buffer);
        return ESP_ERR_NO_MEM;
    }

    memset((void*)t.tx_buffer, 0, 3 + len);
    memcpy((void*)t.tx_buffer, cmd_header, 3);

    esp_err_t ret = spi_device_polling_transmit(s_fram_spi_handle, &t);
    if (ret == ESP_OK) {
        memcpy(data, (uint8_t*)t.rx_buffer + 3, len);
    }

    free((void*)t.tx_buffer);
    free(t.rx_buffer);
    return ret;
}

/* --- Abstraction layer implementation --- */

esp_err_t fram_write_config(const uint8_t *data, size_t len) {
    if (len > 0x0800) return ESP_ERR_INVALID_SIZE;
    return fram_write(FRAM_CONFIG_OFFSET, data, len);
}

esp_err_t fram_read_config(uint8_t *data, size_t len) {
    if (len > 0x0800) return ESP_ERR_INVALID_SIZE;
    return fram_read(FRAM_CONFIG_OFFSET, data, len);
}

esp_err_t fram_write_schedules(const uint8_t *data, size_t len) {
    if (len > 0x1000) return ESP_ERR_INVALID_SIZE;
    return fram_write(FRAM_SCHEDULES_OFFSET, data, len);
}

esp_err_t fram_read_schedules(uint8_t *data, size_t len) {
    if (len > 0x1000) return ESP_ERR_INVALID_SIZE;
    return fram_read(FRAM_SCHEDULES_OFFSET, data, len);
}

esp_err_t fram_write_telemetry_ring(uint16_t relative_offset, const uint8_t *data, size_t len) {
    uint32_t target_addr = FRAM_RING_BUFFER_OFFSET + relative_offset;
    if (target_addr + len > FRAM_TOTAL_SIZE) return ESP_ERR_INVALID_SIZE;
    return fram_write((uint16_t)target_addr, data, len);
}

esp_err_t fram_read_telemetry_ring(uint16_t relative_offset, uint8_t *data, size_t len) {
    uint32_t target_addr = FRAM_RING_BUFFER_OFFSET + relative_offset;
    if (target_addr + len > FRAM_TOTAL_SIZE) return ESP_ERR_INVALID_SIZE;
    return fram_read((uint16_t)target_addr, data, len);
}