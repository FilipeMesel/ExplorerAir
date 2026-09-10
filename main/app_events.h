#ifndef APP_EVENTS_H
#define APP_EVENTS_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "board_mqtt.h"

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

/**
 * @brief Estrutura unificada de eventos da aplicação.
 */
typedef struct {
    app_event_type_t type;
    boot_event_t boot_cause;
    board_mqtt_data_t mqtt_data;
} app_event_t;

/**
 * @brief Fila global de eventos para uso desacoplado entre os serviços.
 */
extern QueueHandle_t g_app_event_queue;

#ifdef __cplusplus
}
#endif

#endif // APP_EVENTS_H