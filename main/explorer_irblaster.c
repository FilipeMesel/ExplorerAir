#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "config.h"
#include "display.h"
#include "wifi_handler.h"
#include "ir_handler.h"

static const char *TAG = "EXPLORER_MAIN";
#define NVS_IR_NAMESPACE "ir_codes"

// Lista exata de comandos a serem aprendidos em sequência
static const char *IR_ACTIONS[] = {
    "LIGAR",
    "DESLIGAR",
    "18 C",
    "19 C",
    "20 C",
    "21 C",
    "22 C",
    "23 C",
    "24 C",
    "25 C"
};
#define TOTAL_ACTIONS (sizeof(IR_ACTIONS) / sizeof(IR_ACTIONS[0]))

// Configura os GPIOs dos botões
static void buttons_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_1_GPIO) | (1ULL << BUTTON_2_GPIO), // GPIO5 e GPIO34
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
}

// Checa se ambos os botões foram pressionados no boot
static bool check_learning_mode_trigger(void) {
    return (gpio_get_level(BUTTON_1_GPIO) != 0) && (gpio_get_level(BUTTON_2_GPIO) != 0);
}

// Salva o buffer raw do IR na NVS
static bool save_ir_to_nvs(const char *key, const ir_raw_command_t *cmd) {
    nvs_handle_t nvs_h;
    esp_err_t err = nvs_open(NVS_IR_NAMESPACE, NVS_READWRITE, &nvs_h);
    if (err != ESP_OK) return false;

    err = nvs_set_blob(nvs_h, key, cmd, sizeof(ir_raw_command_t));
    if (err == ESP_OK) {
        err = nvs_commit(nvs_h);
    }
    nvs_close(nvs_h);
    return (err == ESP_OK);
}

// Rotina de aprendizado do Ar-Condicionado
static void run_ir_learning_routine(void) {
    ESP_LOGI(TAG, "Iniciando modo de aprendizado IR...");
    display_show_text("MODO APRENDIZADO\nINICIANDO...");
    vTaskDelay(pdMS_TO_TICKS(1500));

    ir_raw_command_t captured_cmd;
    bool has_cmd = false;
    char display_buf[64];
    int current_idx = 0;

    // Aguarda soltar os botões antes de começar
    while (gpio_get_level(BUTTON_1_GPIO) != 0 || gpio_get_level(BUTTON_2_GPIO) != 0) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    while (current_idx < TOTAL_ACTIONS) {
        if (!has_cmd) {
            snprintf(display_buf, sizeof(display_buf), "%s\nAGUARDANDO IR...", IR_ACTIONS[current_idx]);
            display_show_text(display_buf);
        }

        // 1. Escuta o receptor RMT IR (permite sobrescrever mandando outro sinal)
        if (ir_read_last_command(&captured_cmd)) {
            has_cmd = true;
            ESP_LOGI(TAG, "Comando capturado para [%s] (Símbolos: %d)", IR_ACTIONS[current_idx], captured_cmd.length);
            
            snprintf(display_buf, sizeof(display_buf), "%s\nIR RECEBIDO!", IR_ACTIONS[current_idx]);
            display_show_text(display_buf);
        }

        // 2. Botão 1 (GPIO5): Testar quantas vezes quiser
        if (gpio_get_level(BUTTON_1_GPIO) != 0) {
            if (has_cmd) {
                snprintf(display_buf, sizeof(display_buf), "%s\nTESTANDO...", IR_ACTIONS[current_idx]);
                display_show_text(display_buf);
                
                // Emite o comando capturado via LED TX
                ir_send_command(&captured_cmd);
                vTaskDelay(pdMS_TO_TICKS(400));

                snprintf(display_buf, sizeof(display_buf), "%s\nIR RECEBIDO!", IR_ACTIONS[current_idx]);
                display_show_text(display_buf);
            } else {
                snprintf(display_buf, sizeof(display_buf), "%s\nNENHUM IR LIDO", IR_ACTIONS[current_idx]);
                display_show_text(display_buf);
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            
            while (gpio_get_level(BUTTON_1_GPIO) != 0) vTaskDelay(pdMS_TO_TICKS(50));
        }

        // 3. Botão 2 (GPIO34): Gravar na NVS e passar para o próximo
        if (gpio_get_level(BUTTON_2_GPIO) != 0) {
            if (has_cmd) {
                // Prepara chave válida para NVS (ex: "LIGAR", "18_C", etc)
                char nvs_key[15];
                snprintf(nvs_key, sizeof(nvs_key), "cmd_%d", current_idx);

                if (save_ir_to_nvs(nvs_key, &captured_cmd)) {
                    ESP_LOGI(TAG, "Comando [%s] gravado na NVS!", IR_ACTIONS[current_idx]);
                    snprintf(display_buf, sizeof(display_buf), "%s\nCOMANDO OK!", IR_ACTIONS[current_idx]);
                } else {
                    ESP_LOGE(TAG, "Erro ao salvar [%s] na NVS!", IR_ACTIONS[current_idx]);
                    snprintf(display_buf, sizeof(display_buf), "%s\nERRO AO SALVAR", IR_ACTIONS[current_idx]);
                }
                display_show_text(display_buf);
                vTaskDelay(pdMS_TO_TICKS(1200));

                current_idx++;
                has_cmd = false;
            } else {
                snprintf(display_buf, sizeof(display_buf), "%s\nENVIE O IR PRIMEIRO", IR_ACTIONS[current_idx]);
                display_show_text(display_buf);
                vTaskDelay(pdMS_TO_TICKS(1000));
            }

            while (gpio_get_level(BUTTON_2_GPIO) != 0) vTaskDelay(pdMS_TO_TICKS(50));
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }

    display_show_text("APRENDIZADO\nCONCLUIDO!");
    ESP_LOGI(TAG, "Todos os comandos de AC foram gravados com sucesso!");
    vTaskDelay(pdMS_TO_TICKS(2000));
}

// Callback do Wi-Fi
static void on_wifi_status_change(const char *title, const char *subtitle) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%s\n%s", title, subtitle);
    display_show_text(buffer);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NEW_VERSION_FOUND || ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    display_init();
    display_power(true);
    buttons_init();
    ir_hardware_init();

    // Se ligar com GPIO5 + GPIO34 pressionados
    if (check_learning_mode_trigger()) {
        run_ir_learning_routine();
    }

    // Fluxo normal de inicialização do Wi-Fi
    int total_attempts = 0;
    const int max_attempts = 6;

    while (total_attempts < max_attempts && !wifi_is_connected() && !wifi_is_failed()) {
        total_attempts++;
        ESP_LOGI(TAG, "Tentativa de conexão Wi-Fi global: %d/%d", total_attempts, max_attempts);
        wifi_connect_init(on_wifi_status_change);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (wifi_is_connected()) {
        ESP_LOGI(TAG, "Wi-Fi conectado com sucesso!");
        display_show_text("CONECTADO");
    } else {
        ESP_LOGE(TAG, "Falha permanente de Wi-Fi.");
        display_show_text("ERRO WIFI");
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}