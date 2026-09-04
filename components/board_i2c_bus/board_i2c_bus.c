/**
 * @file board_i2c_bus.c
 * @brief Implementation of the Thread-safe Shared I2C Bus Manager for ESP-IDF v6.0+
 * @author Filipe Mesel Lobo Costa Cardoso
 * @date 2026-09-03
 * 
 * @copyright Copyright (c) 2026
 */

#include "board_i2c_bus.h"
#include "esp_log.h"

static const char *TAG = "BOARD_I2C_BUS";

static i2c_master_bus_handle_t s_i2c_bus_handle = NULL;     /**< Handle from I2C Bus */
static SemaphoreHandle_t s_i2c_mutex = NULL;                /**< Handle from Mutex for Thread-Safety */

esp_err_t board_i2c_bus_init(void) {
    if (s_i2c_bus_handle != NULL) {
        ESP_LOGW(TAG, "Barramento I2C já se encontra inicializado.");
        return ESP_OK;
    }

    //--------------------------
    // create Mutex for Thread-Safety
    //--------------------------
    s_i2c_mutex = xSemaphoreCreateMutex();
    if (s_i2c_mutex == NULL) {
        ESP_LOGE(TAG, "Falha ao criar o Mutex do I2C!");
        return ESP_ERR_NO_MEM;
    }

    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = BOARD_I2C_PORT_NUM,
        .scl_io_num = BOARD_I2C_SCL_PIN,
        .sda_io_num = BOARD_I2C_SDA_PIN,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&i2c_bus_config, &s_i2c_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar i2c_new_master_bus: %s", esp_err_to_name(ret));
        vSemaphoreDelete(s_i2c_mutex);
        s_i2c_mutex = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "Barramento I2C Master (SDA:%d, SCL:%d) inicializado com sucesso!", 
             BOARD_I2C_SDA_PIN, BOARD_I2C_SCL_PIN);
    return ESP_OK;
}

esp_err_t board_i2c_bus_deinit(void) {
    if (s_i2c_bus_handle == NULL) {
        return ESP_OK;
    }

    esp_err_t ret = i2c_del_master_bus(s_i2c_bus_handle);
    if (ret == ESP_OK) {
        s_i2c_bus_handle = NULL;
        if (s_i2c_mutex != NULL) {
            vSemaphoreDelete(s_i2c_mutex);
            s_i2c_mutex = NULL;
        }
        ESP_LOGI(TAG, "Barramento I2C encerrado com sucesso.");
    } else {
        ESP_LOGE(TAG, "Falha ao remover o barramento I2C: %s", esp_err_to_name(ret));
    }
    return ret;
}

i2c_master_bus_handle_t board_i2c_bus_get_handle(void) {
    return s_i2c_bus_handle;
}

esp_err_t board_i2c_bus_lock(uint32_t timeout_ms) {
    if (s_i2c_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    TickType_t ticks = (timeout_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    if (xSemaphoreTake(s_i2c_mutex, ticks) == pdTRUE) {
        return ESP_OK;
    }
    ESP_LOGE(TAG, "Timeout ao tentar adquirir a trava (lock) do barramento I2C");
    return ESP_ERR_TIMEOUT;
}

esp_err_t board_i2c_bus_unlock(void) {
    if (s_i2c_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreGive(s_i2c_mutex) == pdTRUE) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t board_i2c_bus_scan(void) {
    if (s_i2c_bus_handle == NULL) {
        ESP_LOGE(TAG, "Barramento não inicializado para escaneamento.");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Iniciando varredura no barramento I2C...");
    uint8_t devices_found = 0;

    board_i2c_bus_lock(1000);
    for (uint8_t addr = 1; addr < 127; addr++) {
        if (i2c_master_probe(s_i2c_bus_handle, addr, -1) == ESP_OK) {
            ESP_LOGI(TAG, " -> Dispositivo I2C localizado no endereço: 0x%02X", addr);
            devices_found++;
        }
    }
    board_i2c_bus_unlock();

    if (devices_found == 0) {
        ESP_LOGW(TAG, "Nenhum dispositivo I2C foi detectado!");
    } else {
        ESP_LOGI(TAG, "Varredura concluída. Total de %d dispositivo(s) encontrado(s).", devices_found);
    }

    return ESP_OK;
}