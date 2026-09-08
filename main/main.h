#ifndef MAIN_BOOT_H
#define MAIN_BOOT_H

#include "esp_err.h"
#include <stdbool.h>

// 1. Tipos e Estruturas da Aplicação
#include "app_structs.h"

// 2. Drivers e Periféricos
#include "board_i2c_bus.h"
#include "rtc_ht8563.h"
#include "display_oled.h"
#include "board_wifi.h"
#include "board_mqtt.h"

// 3. Módulos Dependentes dos Tipos Globais
#include "app_storage.h"
#include "json_protocol.h"

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
 * @brief System Unified Application Events for Main Central Queue
 */
typedef enum {
    APP_EVENT_BOOT_ANALYZED,
    APP_EVENT_WIFI_CONNECTED,
    APP_EVENT_WIFI_FAILOVER_EXHAUSTED,
    APP_EVENT_MQTT_CONNECTED,
    APP_EVENT_MQTT_DISCONNECTED,
    APP_EVENT_MQTT_DATA_RECEIVED,
    APP_EVENT_TIMER_SET_SUCCESS,
    APP_EVENT_SHUTDOWN_REQUESTED
} app_event_type_t;


// 2. Definir a struct do evento que faltava
typedef struct {
    app_event_type_t type;
    boot_event_t boot_cause;
    board_mqtt_data_t mqtt_data; // <--- Alterado de board_mqtt_event_data_t para board_mqtt_data_t
} app_event_t;


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