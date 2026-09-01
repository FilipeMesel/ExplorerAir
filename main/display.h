/**
 * @file display.h
 * @author Filipe Mesel Lobo Costa Cardoso
 * @brief This file contains the function declarations for controlling the OLED display.
 * @version 0.1
 * @date 2026-08-31
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize the display
 * 
 * @return void
 */
void display_init(void);

/**
 * @brief Turn the display on or off
 * 
 * @param turn_on true to turn on, false to turn off
 * @return void
 */
void display_power(bool turn_on);

/**
 * @brief Clear the display
 * 
 * @return void
 */
void display_clear(void);

/**
 * @brief Show text on the display
 * 
 * @param text The text to show
 * @return void
 */
void display_show_text(const char *text);

#endif