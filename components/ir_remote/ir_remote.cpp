#include "ir_remote.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Inclusão dos cabeçalhos C++ da biblioteca IRremoteIDF
#include "IRrecv.h"
#include "IRsend.h"
#include "IRutils.h"

static const char *TAG = "IR_REMOTE";

// Instâncias estáticas para RX e TX
static IRrecv *s_ir_recv = nullptr;
static IRsend *s_ir_send = nullptr;
static decode_results s_rx_results;

esp_err_t ir_remote_init(int gpio_tx, int gpio_rx) {
    ESP_LOGI(TAG, "Inicializando biblioteca IRremoteIDF - TX: %d, RX: %d", gpio_tx, gpio_rx);

    // Alocação e inicialização do emissor IR (TX)
    if (s_ir_send != nullptr) {
        delete s_ir_send;
    }
    s_ir_send = new IRsend(static_cast<uint16_t>(gpio_tx));
    if (!s_ir_send) {
        ESP_LOGE(TAG, "Falha ao alocar memória para IRsend");
        return ESP_ERR_NO_MEM;
    }
    s_ir_send->begin();

    // Alocação e inicialização do receptor IR (RX)
    // bufsize=1024 cobre até 700 elementos de timing de forma segura
    if (s_ir_recv != nullptr) {
        delete s_ir_recv;
    }
    s_ir_recv = new IRrecv(static_cast<uint16_t>(gpio_rx), 1024, 50, true);
    if (!s_ir_recv) {
        ESP_LOGE(TAG, "Falha ao alocar memória para IRrecv");
        delete s_ir_send;
        s_ir_send = nullptr;
        return ESP_ERR_NO_MEM;
    }
#if DECODE_HASH
    // /// Ignore "UNKNOWN" messages shorter than this.
    static const uint16_t kMinUnknownSize = 12;
    s_ir_recv->setUnknownThreshold(kMinUnknownSize);
#endif  // DECODE_HASH
    s_ir_recv->setTolerance(45);

    s_ir_recv->enableIRIn();
    ir_remote_resume_ir_receiver();

    ESP_LOGI(TAG, "Driver IRremoteIDF inicializado com sucesso.");
    return ESP_OK;
}

esp_err_t ir_remote_read_last_command(ir_raw_command_t *cmd_out) {
    if (!cmd_out) return ESP_ERR_INVALID_ARG;
    if (!s_ir_recv) {
        ESP_LOGE(TAG, "Receptor IR não foi inicializado.");
        return ESP_ERR_INVALID_STATE;
    }

    // Leitura não-bloqueante utilizando decode
    if (s_ir_recv->decode(&s_rx_results)) {
        // Quantidade de elementos no buffer RAW capturado
        uint16_t raw_len = s_rx_results.rawlen - 1; // Desconsidera o gap inicial se aplicável pela lib

        if (raw_len > MAX_IR_BUFFER_SIZE) {
            ESP_LOGW(TAG, "Comando IR capturado excedeu o limite máximo (%d > %d). Truncando.", 
                     raw_len, MAX_IR_BUFFER_SIZE);
            raw_len = MAX_IR_BUFFER_SIZE;
            return ESP_ERR_NOT_FOUND;
        }

        cmd_out->length = raw_len;

        // Copia os valores em microssegundos do buffer da lib para o array struct
        for (uint16_t i = 0; i < raw_len; i++) {
            cmd_out->data[i] = s_rx_results.rawbuf[i + 1] * kRawTick;
        }

        ESP_LOGI(TAG, "Sinal IR capturado com sucesso (%d pulsos).", cmd_out->length);

        // Prepara o receptor para a próxima leitura
        ir_remote_resume_ir_receiver();
        return ESP_OK;
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t ir_remote_send_command(const ir_raw_command_t *cmd) {
    if (!cmd || cmd->length == 0 || cmd->length > MAX_IR_BUFFER_SIZE) {
        ESP_LOGE(TAG, "Parâmetro de envio inválido ou buffer vazio.");
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_ir_send) {
        ESP_LOGE(TAG, "Emissor IR não foi inicializado.");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Transmitindo %d pulsos RAW em 38kHz...", cmd->length);

    // Pausa recepção durante envio para evitar interferências/loopback
    if (s_ir_recv) {
        s_ir_recv->disableIRIn();
    }

    // Envio raw com frequência portadora padrão de 38kHz
    s_ir_send->sendRaw(cmd->data, cmd->length, 38);

    // Reativa a recepção
    if (s_ir_recv) {
        s_ir_recv->enableIRIn();
        ir_remote_resume_ir_receiver();
    }

    return ESP_OK;
}

esp_err_t ir_remote_resume_ir_receiver()
{
    if (!s_ir_recv) {
        ESP_LOGE(TAG, "Receptor IR não foi inicializado.");
        return ESP_ERR_INVALID_STATE;
    }

    s_ir_recv->resume();
    ESP_LOGI(TAG, "Receptor IR reativado com sucesso.");
    return ESP_OK;
}