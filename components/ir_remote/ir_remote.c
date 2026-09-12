#include "ir_remote.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_rx.h"
#include "esp_log.h"

static const char *TAG = "IR_REMOTE";

static QueueHandle_t s_ir_rx_queue = NULL;
static rmt_channel_handle_t s_tx_channel = NULL;
static rmt_channel_handle_t s_rx_channel = NULL;
static rmt_encoder_handle_t s_copy_encoder = NULL;

static rmt_symbol_word_t s_rx_raw_symbols[MAX_BUFFER_SYMBOLS];

static rmt_receive_config_t s_rx_config = {
    .signal_range_min_ns = 1250,
    .signal_range_max_ns = 30 * 1000 * 1000, // Timeout de 30ms
};

static bool IRAM_ATTR ir_rx_done_callback(rmt_channel_handle_t rx_chan, const rmt_rx_done_event_data_t *edata, void *user_ctx) {
    BaseType_t high_task_wakeup = pdFALSE;
    xQueueSendFromISR(s_ir_rx_queue, edata, &high_task_wakeup);
    return high_task_wakeup == pdTRUE;
}

esp_err_t ir_remote_init(int gpio_tx, int gpio_rx) {
    s_ir_rx_queue = xQueueCreate(5, sizeof(rmt_rx_done_event_data_t));
    if (!s_ir_rx_queue) {
        ESP_LOGE(TAG, "Falha ao criar fila RX");
        return ESP_ERR_NO_MEM;
    }

    // Configuração do Canal RX
    rmt_rx_channel_config_t rx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = gpio_rx,
        .mem_block_symbols = 64,
        .resolution_hz = IR_RESOLUTION_HZ,
        .flags.invert_in = true,
    };
    ESP_ERROR_CHECK(rmt_new_rx_channel(&rx_chan_config, &s_rx_channel));

    rmt_rx_event_callbacks_t cbs = { .on_recv_done = ir_rx_done_callback };
    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(s_rx_channel, &cbs, NULL));
    ESP_ERROR_CHECK(rmt_enable(s_rx_channel));

    // Inicia primeira recepção
    ESP_ERROR_CHECK(rmt_receive(s_rx_channel, s_rx_raw_symbols, sizeof(s_rx_raw_symbols), &s_rx_config));

    // Configuração do Canal TX
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = gpio_tx,
        .mem_block_symbols = 64,
        .resolution_hz = IR_RESOLUTION_HZ,
        .trans_queue_depth = 4,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &s_tx_channel));

    rmt_carrier_config_t carrier_config = {
        .frequency_hz = CARRIER_FREQ_HZ,
        .duty_cycle = 0.33f,
        .flags.polarity_active_low = false,
    };
    ESP_ERROR_CHECK(rmt_apply_carrier(s_tx_channel, &carrier_config));

    rmt_copy_encoder_config_t copy_encoder_config = {};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&copy_encoder_config, &s_copy_encoder));
    ESP_ERROR_CHECK(rmt_enable(s_tx_channel));

    ESP_LOGI(TAG, "Driver IR remoto inicializado nos pinos TX: %d, RX: %d", gpio_tx, gpio_rx);
    return ESP_OK;
}

esp_err_t ir_remote_read_last_command(ir_raw_command_t *cmd_out) {
    if (!cmd_out) return ESP_ERR_INVALID_ARG;

    rmt_rx_done_event_data_t rx_event_data;
    
    if (xQueueReceive(s_ir_rx_queue, &rx_event_data, 0) == pdTRUE) {
        size_t count = rx_event_data.num_symbols;
        
        if (count > 0) {
            cmd_out->length = count * 2;
            if (cmd_out->length > MAX_IR_BUFFER_SIZE) {
                cmd_out->length = MAX_IR_BUFFER_SIZE;
            }

            for (size_t i = 0; i < count && (i * 2 + 1) < MAX_IR_BUFFER_SIZE; i++) {
                cmd_out->data[i * 2]     = rx_event_data.received_symbols[i].duration0;
                cmd_out->data[i * 2 + 1] = rx_event_data.received_symbols[i].duration1;
            }

            rmt_receive(s_rx_channel, s_rx_raw_symbols, sizeof(s_rx_raw_symbols), &s_rx_config);
            return ESP_OK;
        }

        rmt_receive(s_rx_channel, s_rx_raw_symbols, sizeof(s_rx_raw_symbols), &s_rx_config);
    }
    
    return ESP_ERR_NOT_FOUND;
}

esp_err_t ir_remote_send_command(const ir_raw_command_t *cmd) {
    if (!cmd || cmd->length == 0) return ESP_ERR_INVALID_ARG;

    size_t symbols_needed = cmd->length / 2;
    rmt_symbol_word_t tx_symbols[symbols_needed];

    for (size_t i = 0; i < symbols_needed; i++) {
        tx_symbols[i].duration0 = cmd->data[i * 2];
        tx_symbols[i].level0 = 1;
        tx_symbols[i].duration1 = cmd->data[i * 2 + 1];
        tx_symbols[i].level1 = 0;
    }

    rmt_transmit_config_t transmit_config = { .loop_count = 0 };
    esp_err_t ret = rmt_transmit(s_tx_channel, s_copy_encoder, tx_symbols, sizeof(tx_symbols), &transmit_config);
    if (ret == ESP_OK) {
        rmt_tx_wait_all_done(s_tx_channel, portMAX_DELAY);
    }
    
    return ret;
}