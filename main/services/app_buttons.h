#ifndef APP_BUTTONS_H
#define APP_BUTTONS_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Initializes and launches the button-reading task for menu navigation and dual-hold functions.
 * 
 * This task manages navigation across the IR Learning and Test screens using
 * the SELECT and ENTER buttons, while also monitoring for a simultaneous
 * 10-second press to trigger the output and telemetry transmission.
 * 
 * @return esp_err_t ESP_OK upon successful task launch, or ESP_FAIL.
 */
esp_err_t app_buttons_start_menu_task(void);

#ifdef __cplusplus
}
#endif

#endif // APP_BUTTONS_H