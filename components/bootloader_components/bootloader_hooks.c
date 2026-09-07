#include "esp_attr.h"
#include "soc/gpio_struct.h"
#include "soc/gpio_sig_map.h"
#include "esp_rom_gpio.h"

// Executado nos primeiros milissegundos do Bootloader
void bootloader_after_init(void) {
    // 1. Configura o pino 22 para função GPIO
    esp_rom_gpio_pad_select_gpio(22);

    // 2. Conecta o sinal de saída GPIO
    esp_rom_gpio_connect_out_signal(22, SIG_GPIO_OUT_IDX, false, false);

    // 3. Habilita a saída do GPIO 22
    GPIO.enable_w1ts = (1ULL << 22);

    // 4. Força o nível lógico imediatamente para 0 (LOW)
    GPIO.out_w1tc = (1ULL << 22);
}