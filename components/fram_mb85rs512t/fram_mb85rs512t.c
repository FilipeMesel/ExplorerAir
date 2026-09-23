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

esp_err_t fram_erase_all(uint8_t erase_value) {
    #define ERASE_CHUNK_SIZE 4096

    uint8_t *buffer = heap_caps_malloc(ERASE_CHUNK_SIZE, MALLOC_CAP_DMA);
    if (!buffer) {
        ESP_LOGE(TAG, "Falha ao alocar memória DMA para apagar a FRAM");
        return ESP_ERR_NO_MEM;
    }

    memset(buffer, erase_value, ERASE_CHUNK_SIZE);

    esp_err_t ret = ESP_OK;
    ESP_LOGI(TAG, "Iniciando limpeza total da FRAM (%d KB)...", FRAM_TOTAL_SIZE / 1024);

    for (uint32_t addr = 0; addr < FRAM_TOTAL_SIZE; addr += ERASE_CHUNK_SIZE) {
        ret = fram_write((uint16_t)addr, buffer, ERASE_CHUNK_SIZE);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Falha ao apagar o bloco no endereço 0x%04X (Erro: %s)", (unsigned int)addr, esp_err_to_name(ret));
            break;
        }
    }

    free(buffer);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "FRAM apagada com sucesso (valor preenchido: 0x%02X)", erase_value);
    }

    return ret;
}

/* --- Embedded Self-Test Routine --- */

esp_err_t fram_run_tests(void) {
    ESP_LOGI(TAG, "=================================================");
    ESP_LOGI(TAG, "      EXECUTANDO TESTES DA MEMÓRIA FRAM          ");
    ESP_LOGI(TAG, "=================================================");

    bool test_passed = true;

    // Test 1: Reading Status Register (RDSR)
    uint8_t status_reg = 0;
    if (fram_read_status(&status_reg) == ESP_OK) {
        ESP_LOGI(TAG, "[TESTE 1] Status Register obtido: 0x%02X", status_reg);
    } else {
        ESP_LOGE(TAG, "[TESTE 1] Falha ao ler Status Register");
        test_passed = false;
    }

    ESP_LOGI(TAG, "=================================================");
    return test_passed ? ESP_OK : ESP_FAIL;
}