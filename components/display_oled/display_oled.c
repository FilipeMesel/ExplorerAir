/**
 * @file display_oled.c
 * @brief Implementation of OLED Driver and UI layout rendering.
 * @author Filipe Mesel Lobo Costa Cardoso
 * @date 2026
 */

#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board_i2c_bus.h"
#include "display_oled.h"
#include "sdkconfig.h"

static const char *TAG = "DISPLAY_OLED";

static i2c_master_dev_handle_t s_oled_dev_handle = NULL;
static uint8_t s_framebuffer[OLED_WIDTH * OLED_HEIGHT / 8];

static uint8_t s_battery_pct = 100;
static char s_fw_version[8] = "v1.0";

/* AC Actions Map */
static const char *s_action_strings[OLED_CMD_MAX] = {
    [OLED_CMD_POWER_OFF] = "DESLIGAR",                  /*< Turn Off */
    [OLED_CMD_POWER_ON]  = "LIGAR",                     /*< Turn On */
    [OLED_CMD_TEMP_18]   = "18 Graus",                  /*< Temperature 18 C */
    [OLED_CMD_TEMP_19]   = "19 Graus",                  /*< Temperature 19 C */
    [OLED_CMD_TEMP_20]   = "20 Graus",                  /*< Temperature 20 C */
    [OLED_CMD_TEMP_21]   = "21 Graus",                  /*< Temperature 21 C */
    [OLED_CMD_TEMP_22]   = "22 Graus",                  /*< Temperature 22 C */
    [OLED_CMD_TEMP_23]   = "23 Graus",                  /*< Temperature 23 C */
    [OLED_CMD_TEMP_24]   = "24 Graus",                  /*< Temperature 24 C */
    [OLED_CMD_TEMP_25]   = "25 Graus"                   /*< Temperature 25 C */
};

/**
 * @brief Array containing the font data for characters 32 to 126 in a 5x7 pixel matrix.
 * 
 */
static const uint8_t font5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // ' '
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // "
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // #
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // '
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // (
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // )
    {0x08, 0x2A, 0x1C, 0x2A, 0x08}, // *
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // /
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // ;
    {0x08, 0x14, 0x22, 0x41, 0x00}, // <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // =
    {0x00, 0x41, 0x22, 0x14, 0x08}, // >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // ?
    {0x32, 0x49, 0x79, 0x41, 0x3E}, // @
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}  // Z
};

/**
 * @brief Array containing the font data for characters 32 to 126 in a 3x5 pixel matrix.
 * 
 */
static const uint8_t font3x5[][3] = {
    {0x0, 0x0, 0x0}, // ' '
    {0x7, 0x5, 0x7}, // 0
    {0x0, 0x7, 0x0}, // 1
    {0x5, 0x5, 0x7}, // 2
    {0x5, 0x5, 0x7}, // 3
    {0x7, 0x1, 0x7}, // 4
    {0x7, 0x5, 0x5}, // 5
    {0x7, 0x5, 0x5}, // 6
    {0x1, 0x1, 0x7}, // 7
    {0x7, 0x5, 0x7}, // 8
    {0x7, 0x5, 0x7}, // 9
    {0x3, 0x4, 0x3}, // v
    {0x0, 0x2, 0x0}, // .
    {0x5, 0x2, 0x5}  // %
};

static void draw_pixel(int x, int y, bool color) {
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) return;
    if (color) {
        s_framebuffer[x + (y / 8) * OLED_WIDTH] |= (1 << (y % 8));
    } else {
        s_framebuffer[x + (y / 8) * OLED_WIDTH] &= ~(1 << (y % 8));
    }
}

static void draw_char_5x7(int x, int y, char c) {
    if (c < 32 || c > 90) c = '?';
    uint8_t idx = c - 32;
    for (int col = 0; col < 5; col++) {
        uint8_t line = font5x7[idx][col];
        for (int row = 0; row < 7; row++) {
            draw_pixel(x + col, y + row, (line >> row) & 0x01);
        }
    }
}

static void draw_string_5x7(int x, int y, const char *str) {
    while (*str) {
        draw_char_5x7(x, y, *str);
        x += 6;
        str++;
    }
}

static void draw_char_3x5(int x, int y, char c) {
    uint8_t idx = 0;
    if (c >= '0' && c <= '9') idx = c - '0' + 1;
    else if (c == 'v' || c == 'V') idx = 11;
    else if (c == '.') idx = 12;
    else if (c == '%') idx = 13;

    for (int col = 0; col < 3; col++) {
        uint8_t line = font3x5[idx][col];
        for (int row = 0; row < 5; row++) {
            draw_pixel(x + col, y + row, (line >> row) & 0x01);
        }
    }
}

static void draw_string_3x5(int x, int y, const char *str) {
    while (*str) {
        draw_char_3x5(x, y, *str);
        x += 4;
        str++;
    }
}

static esp_err_t oled_write_cmd(uint8_t cmd) {
    uint8_t buf[2] = {0x00, cmd};
    return i2c_master_transmit(s_oled_dev_handle, buf, sizeof(buf), 100);
}

static esp_err_t oled_flush(void) {
    board_i2c_bus_lock(100);

    // Setting the memory pointer to the start of the display RAM
    oled_write_cmd(0x21); // Set Column Address
    oled_write_cmd(0);
    oled_write_cmd(127);
    oled_write_cmd(0x22); // Set Page Address
    oled_write_cmd(0);
    oled_write_cmd(7);

    uint8_t tx_buf[1025];
    tx_buf[0] = 0x40; // Co=0, D/C=1 (Data Mode)
    memcpy(&tx_buf[1], s_framebuffer, sizeof(s_framebuffer));

    esp_err_t ret = i2c_master_transmit(s_oled_dev_handle, tx_buf, sizeof(tx_buf), 200);
    board_i2c_bus_unlock();
    return ret;
}

static void render_header(void) {
    // 1. Top Left Corner: Version (e.g., "v1.0")
    draw_string_5x7(0, 0, s_fw_version);

    // 2. Top Right Corner: Battery Percentage (e.g., "100%")
    char bat_str[8];
    snprintf(bat_str, sizeof(bat_str), "%d%%", s_battery_pct);
    
    // Precise calculation of the x-position for right alignment (each 5x7 character occupies 6px of width)
    int bat_x = OLED_WIDTH - (strlen(bat_str) * 6);
    draw_string_5x7(bat_x > 0 ? bat_x : 0, 0, bat_str);

    // 3. Horizontal Division Line below the Header (y = 9)
    for (int x = 0; x < OLED_WIDTH; x++) {
        draw_pixel(x, 9, true);
    }
}

esp_err_t oled_init(uint8_t i2c_addr) {
    i2c_master_bus_handle_t bus_handle = board_i2c_bus_get_handle();
    ESP_RETURN_ON_FALSE(bus_handle != NULL, ESP_ERR_INVALID_STATE, TAG, "Barramento I2C não inicializado");

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = i2c_addr,
        .scl_speed_hz = 400000,
    };

    board_i2c_bus_lock(100);
    esp_err_t ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &s_oled_dev_handle);
    if (ret != ESP_OK) {
        board_i2c_bus_unlock();
        ESP_LOGE(TAG, "Falha ao adicionar dispositivo OLED ao barramento I2C");
        return ret;
    }

    // SSD1306 (128x64) initialization command sequence
    uint8_t init_cmds[] = {
        0xAE,       // Display OFF
        0xD5, 0x80, // Clock Divide Ratio
        0xA8, 0x3F, // Multiplex Ratio (64 lines)
        0xD3, 0x00, // Display Offset
        0x40,       // Start Line
        0x8D, 0x14, // Charge Pump Enable
        0x20, 0x00, // Horizontal Addressing Mode
        0xA1,       // Segment Remap (flip X)
        0xC8,       // COM Scan Direction (flip Y)
        0xDA, 0x12, // COM Pins Hardware Config
        0x81, 0xCF, // Contrast Control
        0xD9, 0xF1, // Pre-charge Period
        0xDB, 0x40, // VCOMH Deselect Level
        0xA4,       // Output Follows RAM
        0xA6,       // Normal Display
        0xAF        // Display ON
    };

    for (size_t i = 0; i < sizeof(init_cmds); i++) {
        oled_write_cmd(init_cmds[i]);
    }
    board_i2c_bus_unlock();

    ESP_LOGI(TAG, "Display OLED inicializado com sucesso no endereço 0x%02X", i2c_addr);
    return oled_clear();
}

esp_err_t oled_deinit(void) {
    if (s_oled_dev_handle != NULL) {
        esp_err_t ret = i2c_master_bus_rm_device(s_oled_dev_handle);
        if (ret == ESP_OK) {
            s_oled_dev_handle = NULL;
            ESP_LOGI(TAG, "Dispositivo OLED removido do barramento I2C.");
        }
        return ret;
    }
    return ESP_OK;
}

esp_err_t oled_clear(void) {
    memset(s_framebuffer, 0x00, sizeof(s_framebuffer));
    return oled_flush();
}

void oled_set_header_info(uint8_t battery_percentage, const char *fw_version) {
    if (battery_percentage <= 100) s_battery_pct = battery_percentage;
    if (fw_version) {
        strncpy(s_fw_version, fw_version, sizeof(s_fw_version) - 1);
    }
}

esp_err_t oled_show_message(const char *line1, const char *line2) {
    memset(s_framebuffer, 0x00, sizeof(s_framebuffer));
    render_header();

    if (line1) {
        int x1 = (OLED_WIDTH - (strlen(line1) * 6)) / 2;
        draw_string_5x7(x1 > 0 ? x1 : 0, 24, line1);
    }

    if (line2) {
        int x2 = (OLED_WIDTH - (strlen(line2) * 6)) / 2;
        draw_string_5x7(x2 > 0 ? x2 : 0, 44, line2);
    }

    return oled_flush();
}

esp_err_t oled_show_screen(oled_screen_t screen, oled_cmd_action_t action, uint8_t selected_index) {
    memset(s_framebuffer, 0x00, sizeof(s_framebuffer));
    render_header();

    switch (screen) {
        case OLED_SCREEN_MENU_MAIN:
            draw_string_5x7(10, 24, selected_index == 0 ? "> 1. APRENDER" : "  1. APRENDER");
            draw_string_5x7(10, 44, selected_index == 1 ? "> 2. TESTAR"   : "  2. TESTAR");
            break;

        case OLED_SCREEN_IR_LEARN:
            draw_string_5x7(15, 22, "APRENDER IR:");
            if (action < OLED_CMD_MAX) {
                int x = (OLED_WIDTH - (strlen(s_action_strings[action]) * 6)) / 2;
                draw_string_5x7(x > 0 ? x : 0, 42, s_action_strings[action]);
            }
            break;

        case OLED_SCREEN_IR_TEST:
            draw_string_5x7(20, 22, "TESTAR IR:");
            if (action < OLED_CMD_MAX) {
                int x = (OLED_WIDTH - (strlen(s_action_strings[action]) * 6)) / 2;
                draw_string_5x7(x > 0 ? x : 0, 42, s_action_strings[action]);
            }
            break;

        case OLED_SCREEN_WIFI_ERROR:
            draw_string_5x7(25, 24, "ERRO WIFI");
            draw_string_5x7(15, 44, "SEM CONEXAO!");
            break;

        case OLED_SCREEN_BOOT:
            draw_string_5x7(20, 32, "INICIANDO...");
            break;

        case OLED_SCREEN_SLEEP_PREP:
            draw_string_5x7(20, 32, "ENTRANDO SLEEP");
            break;

        default:
            break;
    }

    return oled_flush();
}

void oled_run_tests(void) {
    ESP_LOGW(TAG, "=================================================");
    ESP_LOGW(TAG, "      EXECUTANDO TESTES DO DISPLAY OLED          ");
    ESP_LOGW(TAG, "=================================================");

    oled_set_header_info(85, "v1.0");

    // Teste 1: Main menu rendering with both options
    ESP_LOGI(TAG, "[OLED TEST] Renderizando Menu Principal (Opcao 1)");
    oled_show_screen(OLED_SCREEN_MENU_MAIN, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_LOGI(TAG, "[OLED TEST] Renderizando Menu Principal (Opcao 2)");
    oled_show_screen(OLED_SCREEN_MENU_MAIN, 0, 1);
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Teste 2: IR Command Sequence
    for (int i = OLED_CMD_POWER_OFF; i < OLED_CMD_MAX; i++) {
        ESP_LOGI(TAG, "[OLED TEST] Renderizando Acao IR: %s", s_action_strings[i]);
        oled_show_screen(OLED_SCREEN_IR_LEARN, (oled_cmd_action_t)i, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // Teste 3: Wi-Fi Error Screen
    ESP_LOGI(TAG, "[OLED TEST] Renderizando Tela de Erro Wi-Fi");
    oled_show_screen(OLED_SCREEN_WIFI_ERROR, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP_LOGW(TAG, "=================================================");
}