#include "esp_log.h"
#include "esp_rom_gpio.h"    // Correct header in ESP-IDF v6.0 for esp_rom_gpio_*
#include "hal/gpio_hal.h"    // Low-level HAL layer for GPIO control

#define MEU_GPIO_PINO 22

// Forces the linker to include this file in the bootloader.
void bootloader_hooks_include(void) { }

// Internal helper function to configure and reset the GPIO.
static void configurar_e_zerar_gpio(void) {
    // 1. Disconnects the peripheral pin and configures it as an output via the ROM API.
    esp_rom_gpio_pad_select_gpio(MEU_GPIO_PINO);
    esp_rom_gpio_connect_out_signal(MEU_GPIO_PINO, SIG_GPIO_OUT_IDX, false, false);

    // 2. Enables the output function in the low-level HAL structure.
    gpio_hal_context_t gpio_hal = { .dev = GPIO_LL_GET_HW(GPIO_PORT_0) };
    gpio_hal_output_enable(&gpio_hal, MEU_GPIO_PINO);

    // 3. Sets the logic level to LOW (0)
    gpio_hal_set_level(&gpio_hal, MEU_GPIO_PINO, 0);
}

// Executes before the 2nd-stage bootloader starts.
void bootloader_before_init(void) {
    configurar_e_zerar_gpio();
    ESP_LOGI("HOOK", "GPIO %d configurado para LOW ANTES do bootloader.", MEU_GPIO_PINO);
}

// Executes after the bootloader initialization, right before app_main.
void bootloader_after_init(void) {
    configurar_e_zerar_gpio();
    ESP_LOGI("HOOK", "GPIO %d garantido em LOW DEPOIS do bootloader.", MEU_GPIO_PINO);
}