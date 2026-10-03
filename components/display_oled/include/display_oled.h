/**
 * @file display_oled.h
 * @brief SSD1306 Low-Level Graphics Driver over Shared I2C Bus.
 * @author Embedded Software Team
 * @date 2026
 */

#ifndef DISPLAY_OLED_H
#define DISPLAY_OLED_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OLED_I2C_ADDR_DEFAULT   0x3C    /**< Default I2C slave address for SSD1306 */
#define OLED_WIDTH              128     /**< Display width in pixels */
#define OLED_HEIGHT             64      /**< Display height in pixels */

/**
 * @brief Initialize the OLED display controller on the shared I2C bus.
 *
 * @param[in] i2c_addr Target I2C slave address (e.g., 0x3C).
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t oled_init(uint8_t i2c_addr);

/**
 * @brief Deinitialize the OLED display and detach from I2C bus.
 *
 * @return ESP_OK on success.
 */
esp_err_t oled_deinit(void);

/**
 * @brief Clear the internal framebuffer and update display.
 *
 * @return ESP_OK on success.
 */
esp_err_t oled_clear(void);

/**
 * @brief Flush framebuffer memory to the OLED display controller.
 *
 * @return ESP_OK on success.
 */
esp_err_t oled_flush(void);

/* =========================================================================
 * GRAPHICS & DRAWING PRIMITIVES
 * ========================================================================= */

/**
 * @brief Set or clear a pixel in the framebuffer.
 *
 * @param[in] x X coordinate (0..127).
 * @param[in] y Y coordinate (0..63).
 * @param[in] color True for white pixel, false for black pixel.
 */
void oled_draw_pixel(int x, int y, bool color);

/**
 * @brief Draw a horizontal line.
 *
 * @param[in] x Start X coordinate.
 * @param[in] y Y coordinate.
 * @param[in] width Line length in pixels.
 * @param[in] color True for white, false for black.
 */
void oled_draw_hline(int x, int y, int width, bool color);

/**
 * @brief Render a single 5x7 ASCII character.
 *
 * @param[in] x X coordinate.
 * @param[in] y Y coordinate.
 * @param[in] c Character byte.
 */
void oled_draw_char_5x7(int x, int y, char c);

/**
 * @brief Render a string using 5x7 font.
 *
 * @param[in] x X coordinate.
 * @param[in] y Y coordinate.
 * @param[in] str Null-terminated string.
 */
void oled_draw_string_5x7(int x, int y, const char *str);

/**
 * @brief Render a single 3x5 numeric/small character.
 *
 * @param[in] x X coordinate.
 * @param[in] y Y coordinate.
 * @param[in] c Character byte.
 */
void oled_draw_char_3x5(int x, int y, char c);

/**
 * @brief Render a string using 3x5 font.
 *
 * @param[in] x X coordinate.
 * @param[in] y Y coordinate.
 * @param[in] str Null-terminated string.
 */
void oled_draw_string_3x5(int x, int y, const char *str);

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_OLED_H