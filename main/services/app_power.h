#ifndef APP_POWER_H
#define APP_POWER_H

#include "esp_err.h"
#include "app_events.h"
#include "app_structs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the power pins and boot buttons of the power manager.
 * 
 * @return esp_err_t ESP_OK in case of success.
 */
esp_err_t app_power_init(void);

/**
 * @brief Analyzes the GPIOs and RTC flags to determine the cause of the boot.
 * 
 * @param[out] out_event Pointer where the detected boot event will be stored.
 * @return esp_err_t ESP_OK in case of success.
 */
esp_err_t app_power_analyze_boot(boot_event_t *out_event);

/**
 * @brief Evaluates schedules and telemetry to program the next event (TF/AF) on the RTC HT8563.
 * 
 * @return esp_err_t ESP_OK in case of success.
 */
esp_err_t app_power_schedule_next_wakeup(void);

/**
 * @brief Performs a complete system shutdown: schedules the next wakeup, stops connectivity, and cuts the HOLD GPIO signal.
 */
void app_power_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // APP_POWER_H