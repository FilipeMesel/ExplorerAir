#include "ir_handler.h"
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"

// Fila armazena o evento de recepção
static QueueHandle_t ir_rx_queue = NULL;

static rmt_channel_handle_t tx_channel = NULL;
static rmt_channel_handle_t rx_channel = NULL;
static rmt_encoder_handle_t copy_encoder = NULL;

// O buffer de símbolos RX precisa ser ESTÁTICO e PERSISTENTE para o driver RMT
static rmt_symbol_word_t rx_raw_symbols[MAX_BUFFER_SYMBOLS];

static rmt_receive_config_t rx_config = {
    .signal_range_min_ns = 0,
    .signal_range_max_ns = 30 * 1000 * 1000, // Timeout de 30ms (silêncio)
};

static bool IRAM_ATTR ir_rx_done_callback(rmt_channel_handle_t rx_chan, const rmt_rx_done_event_data_t *edata, void *user_ctx) {
    BaseType_t high_task_wakeup = pdFALSE;
    // Envia o evento recebido para a fila principal
    xQueueSendFromISR(ir_rx_queue, edata, &high_task_wakeup);
    return high_task_wakeup == pdTRUE;
}

void ir_hardware_init(void) {
    ir_rx_queue = xQueueCreate(5, sizeof(rmt_rx_done_event_data_t));

    // Configuração do canal RX
    rmt_rx_channel_config_t rx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = IR_RECEIVE_PIN,
        .mem_block_symbols = 64,
        .resolution_hz = IR_RESOLUTION_HZ,
        .flags.invert_in = true, // Inverte a lógica para receptores tipo TSOP
    };
    ESP_ERROR_CHECK(rmt_new_rx_channel(&rx_chan_config, &rx_channel));

    rmt_rx_event_callbacks_t cbs = { .on_recv_done = ir_rx_done_callback };
    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(rx_channel, &cbs, NULL));
    ESP_ERROR_CHECK(rmt_enable(rx_channel));

    // Dispara a primeira recepção RMT no buffer estático
    ESP_ERROR_CHECK(rmt_receive(rx_channel, rx_raw_symbols, sizeof(rx_raw_symbols), &rx_config));

    // Configuração do canal TX
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = IR_SEND_PIN,
        .mem_block_symbols = 64,
        .resolution_hz = IR_RESOLUTION_HZ,
        .trans_queue_depth = 4,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &tx_channel));

    rmt_carrier_config_t carrier_config = {
        .frequency_hz = CARRIER_FREQ_HZ,
        .duty_cycle = 0.33,
        .flags.polarity_active_low = false,
    };
    ESP_ERROR_CHECK(rmt_apply_carrier(tx_channel, &carrier_config));

    rmt_copy_encoder_config_t copy_encoder_config = {};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&copy_encoder_config, &copy_encoder));
    ESP_ERROR_CHECK(rmt_enable(tx_channel));
}

bool ir_read_last_command(ir_raw_command_t *cmd_out) {
    rmt_rx_done_event_data_t rx_event_data;
    
    // Tenta obter o pacote lido sem bloquear o fluxo principal
    if (xQueueReceive(ir_rx_queue, &rx_event_data, 0) == pdTRUE) {
        size_t count = rx_event_data.num_symbols;
        
        if (count > 0) {
            cmd_out->length = count * 2;
            if (cmd_out->length > 200) cmd_out->length = 200;

            // Extrai a duração dos pares de pulsos ativos/inativos
            for (size_t i = 0; i < count && (i * 2 + 1) < 200; i++) {
                cmd_out->data[i * 2]     = rx_event_data.received_symbols[i].duration0;
                cmd_out->data[i * 2 + 1] = rx_event_data.received_symbols[i].duration1;
            }

            // Rearma o hardware RMT imediatamente após ler os dados
            rmt_receive(rx_channel, rx_raw_symbols, sizeof(rx_raw_symbols), &rx_config);
            return true;
        }

        // Rearma mesmo se o tamanho for zero
        rmt_receive(rx_channel, rx_raw_symbols, sizeof(rx_raw_symbols), &rx_config);
    }
    return false;
}

void ir_send_command(const ir_raw_command_t *cmd) {
    if (cmd->length == 0) return;

    size_t symbols_needed = cmd->length / 2;
    rmt_symbol_word_t tx_symbols[symbols_needed];

    for (size_t i = 0; i < symbols_needed; i++) {
        tx_symbols[i].duration0 = cmd->data[i * 2];
        tx_symbols[i].level0 = 1;
        tx_symbols[i].duration1 = cmd->data[i * 2 + 1];
        tx_symbols[i].level1 = 0;
    }

    rmt_transmit_config_t transmit_config = { .loop_count = 0 };
    rmt_transmit(tx_channel, copy_encoder, tx_symbols, sizeof(tx_symbols), &transmit_config);
    rmt_tx_wait_all_done(tx_channel, portMAX_DELAY);
}