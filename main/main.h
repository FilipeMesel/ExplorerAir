/**
 * @file main_boot.h
 * @brief Analysis and identification of system wakeup causes (Task 0).
 */

#ifndef MAIN_BOOT_H
#define MAIN_BOOT_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief System Boot / Wakeup Event Types.
 */
typedef enum {
    EVENT_BOOT_POWER_ON = 0,                    /**< Hard Reset or Power-On Reset */
    EVENT_WAKEUP_BUTTON_DUAL_HOLD,             /**< GPIO 5 + GPIO 38 held for >= 2 seconds */
    EVENT_WAKEUP_RTC_TIMER,                    /**< RTC Periodic Telemetry Timer Interrupt (TF) */
    EVENT_WAKEUP_RTC_ALARM,                    /**< RTC Schedule Alarm Interrupt (AF) */
    EVENT_WAKEUP_SCHEDULE_TELEMETRY_CONFLICT,  /**< Timer and Alarm triggered simultaneously */
    EVENT_WAKEUP_SINGLE_BUTTON,                 /**< Single button press for quick screen status */
    EVENT_LOW_BATTERY_SHUTDOWN                 /**< Battery level below critical threshold */
} boot_event_t;

/**
 * @brief Initializes boot analysis pins and determines the current wakeup event.
 * 
 * @param[out] out_event Pointer to store the detected boot event.
 * @return esp_err_t ESP_OK on success, or error code on hardware read failure.
 */
esp_err_t analyze_boot_cause(boot_event_t *out_event);

#ifdef __cplusplus
}
#endif

#endif // MAIN_BOOT_H