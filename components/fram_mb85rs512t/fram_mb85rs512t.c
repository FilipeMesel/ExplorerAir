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

    // Test 2: Configuration Memory Section Write/Read
    const char write_config[] = "CONFIG_IR_BLASTER_V1";
    char read_config[30] = {0};

    ESP_LOGI(TAG, "[TESTE 2] Escrevendo na Seção de Configuração...");
    if (fram_write_config((uint8_t *)write_config, strlen(write_config)) == ESP_OK) {
        fram_read_config((uint8_t *)read_config, strlen(write_config));
        ESP_LOGI(TAG, "[TESTE 2] Lido: %s", read_config);

        if (strcmp(write_config, read_config) == 0) {
            ESP_LOGI(TAG, "[TESTE 2] PASSOU: Leitura/Escrita de Configuração OK!");
        } else {
            ESP_LOGE(TAG, "[TESTE 2] FALHOU: Dados divergentes!");
            test_passed = false;
        }
    } else {
        ESP_LOGE(TAG, "[TESTE 2] FALHOU ao escrever configuração");
        test_passed = false;
    }

    // Test 3: Telemetry Ring Buffer Section
    uint8_t telemetry_data[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint8_t telemetry_read[4] = {0};

    ESP_LOGI(TAG, "[TESTE 3] Escrevendo Telemetria no Ring Buffer...");
    if (fram_write_telemetry_ring(0x00, telemetry_data, sizeof(telemetry_data)) == ESP_OK) {
        fram_read_telemetry_ring(0x00, telemetry_read, sizeof(telemetry_read));

        if (memcmp(telemetry_data, telemetry_read, sizeof(telemetry_data)) == 0) {
            ESP_LOGI(TAG, "[TESTE 3] PASSOU: Ring Buffer de Telemetria OK!");
        } else {
            ESP_LOGE(TAG, "[TESTE 3] FALHOU: Falha na validação da Telemetria!");
            test_passed = false;
        }
    } else {
        ESP_LOGE(TAG, "[TESTE 3] FALHOU ao escrever no Ring Buffer");
        test_passed = false;
    }

    ESP_LOGI(TAG, "=================================================");
    return test_passed ? ESP_OK : ESP_FAIL;
}