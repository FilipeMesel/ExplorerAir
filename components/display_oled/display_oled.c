/**
 * @file display_oled.c
 * @brief Low-level SSD1306 OLED Display Driver implementation.
 */

#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_check.h"
#include "board_i2c_bus.h"
#include "display_oled.h"

static const char *TAG = "DISPLAY_OLED";

static i2c_master_dev_handle_t s_oled_dev_handle = NULL;
static uint8_t s_framebuffer[OLED_WIDTH * OLED_HEIGHT / 8];

/* Font definitions: 5x7 ASCII (32..90) */
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

/* Font definitions: 3x5 Small font */
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

static esp_err_t oled_write_cmd(uint8_t cmd) {
    uint8_t buf[2] = {0x00, cmd};
    return i2c_master_transmit(s_oled_dev_handle, buf, sizeof(buf), 100);
}

void oled_draw_pixel(int x, int y, bool color) {
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) return;
    if (color) {
        s_framebuffer[x + (y / 8) * OLED_WIDTH] |= (1 << (y % 8));
    } else {
        s_framebuffer[x + (y / 8) * OLED_WIDTH] &= ~(1 << (y % 8));
    }
}

void oled_draw_hline(int x, int y, int width, bool color) {
    for (int i = 0; i < width; i++) {
        oled_draw_pixel(x + i, y, color);
    }
}

void oled_draw_char_5x7(int x, int y, char c) {
    if (c < 32 || c > 90) c = '?';
    uint8_t idx = c - 32;
    for (int col = 0; col < 5; col++) {
        uint8_t line = font5x7[idx][col];
        for (int row = 0; row < 7; row++) {
            oled_draw_pixel(x + col, y + row, (line >> row) & 0x01);
        }
    }
}

void oled_draw_string_5x7(int x, int y, const char *str) {
    if (!str) return;
    while (*str) {
        oled_draw_char_5x7(x, y, *str);
        x += 6;
        str++;
    }
}

void oled_draw_char_3x5(int x, int y, char c) {
    uint8_t idx = 0;
    if (c >= '0' && c <= '9') idx = c - '0' + 1;
    else if (c == 'v' || c == 'V') idx = 11;
    else if (c == '.') idx = 12;
    else if (c == '%') idx = 13;

    for (int col = 0; col < 3; col++) {
        uint8_t line = font3x5[idx][col];
        for (int row = 0; row < 5; row++) {
            oled_draw_pixel(x + col, y + row, (line >> row) & 0x01);
        }
    }
}

void oled_draw_string_3x5(int x, int y, const char *str) {
    if (!str) return;
    while (*str) {
        oled_draw_char_3x5(x, y, *str);
        x += 4;
        str++;
    }
}

esp_err_t oled_flush(void) {
    if (s_oled_dev_handle == NULL) return ESP_ERR_INVALID_STATE;

    board_i2c_bus_lock(100);

    oled_write_cmd(0x21); // Set Column Address
    oled_write_cmd(0);
    oled_write_cmd(127);
    oled_write_cmd(0x22); // Set Page Address
    oled_write_cmd(0);
    oled_write_cmd(7);

    uint8_t tx_buf[1025];
    tx_buf[0] = 0x40; // Data Mode
    memcpy(&tx_buf[1], s_framebuffer, sizeof(s_framebuffer));

    esp_err_t ret = i2c_master_transmit(s_oled_dev_handle, tx_buf, sizeof(tx_buf), 200);
    board_i2c_bus_unlock();
    return ret;
}

esp_err_t oled_clear(void) {
    memset(s_framebuffer, 0x00, sizeof(s_framebuffer));
    return oled_flush();
}

esp_err_t oled_init(uint8_t i2c_addr) {
    i2c_master_bus_handle_t bus_handle = board_i2c_bus_get_handle();
    ESP_RETURN_ON_FALSE(bus_handle != NULL, ESP_ERR_INVALID_STATE, TAG, "I2C master bus not initialized");

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = i2c_addr,
        .scl_speed_hz = 400000,
    };

    board_i2c_bus_lock(100);
    esp_err_t ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &s_oled_dev_handle);
    if (ret != ESP_OK) {
        board_i2c_bus_unlock();
        ESP_LOGE(TAG, "Failed to attach OLED device to I2C bus");
        return ret;
    }

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

    ESP_LOGI(TAG, "OLED Display driver initialized on addr 0x%02X", i2c_addr);
    return oled_clear();
}

esp_err_t oled_deinit(void) {
    if (s_oled_dev_handle != NULL) {
        esp_err_t ret = i2c_master_bus_rm_device(s_oled_dev_handle);
        if (ret == ESP_OK) {
            s_oled_dev_handle = NULL;
            ESP_LOGI(TAG, "OLED device detached from I2C bus");
        }
        return ret;
    }
    return ESP_OK;
}