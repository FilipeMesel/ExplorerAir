#include "esp_log.h"
#include "esp_rom_gpio.h"    // Cabeçalho correto no ESP-IDF v6.0 para esp_rom_gpio_*
#include "hal/gpio_hal.h"    // Camada HAL de baixo nível para controle do GPIO

#define MEU_GPIO_PINO 22

// Força o linker a incluir este arquivo no bootloader
void bootloader_hooks_include(void) { }

// Função auxiliar interna para configurar e zerar o GPIO
static void configurar_e_zerar_gpio(void) {
    // 1. Desconecta o pino de periféricos e define como saída via ROM API
    esp_rom_gpio_pad_select_gpio(MEU_GPIO_PINO);
    esp_rom_gpio_connect_out_signal(MEU_GPIO_PINO, SIG_GPIO_OUT_IDX, false, false);

    // 2. Habilita a função de saída (output) na estrutura HAL de baixo nível
    gpio_hal_context_t gpio_hal = { .dev = GPIO_LL_GET_HW(GPIO_PORT_0) };
    gpio_hal_output_enable(&gpio_hal, MEU_GPIO_PINO);

    // 3. Define o nível lógico para BAIXO (0)
    gpio_hal_set_level(&gpio_hal, MEU_GPIO_PINO, 0);
}

// Executa ANTES da inicialização do 2º estágio do bootloader
void bootloader_before_init(void) {
    configurar_e_zerar_gpio();
    ESP_LOGI("HOOK", "GPIO %d configurado para LOW ANTES do bootloader.", MEU_GPIO_PINO);
}

// Executa DEPOIS da inicialização do bootloader, logo antes do app_main
void bootloader_after_init(void) {
    configurar_e_zerar_gpio();
    ESP_LOGI("HOOK", "GPIO %d garantido em LOW DEPOIS do bootloader.", MEU_GPIO_PINO);
}