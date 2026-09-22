#ifndef APP_STRUCTS_H
#define APP_STRUCTS_H

#include <stdint.h>
#include <stdbool.h>
#include "rtc_ht8563.h"
#include "ir_remote.h"

/**
 * @brief Standard maximum sizes for Wi-Fi credentials
 */
#define WIFI_SSID_MAX_LEN           32  /**< WIFI SSID Maximum length */
#define WIFI_PASS_MAX_LEN           64  /**< WIFI PASSWORD Maximum length */
#define MAX_SCHEDULE_ITEMS          11  /**< Schedules from ID 0 to ID 10 */
#define SCHEDULE_TIME_STR_LEN       6   /**< "HH:MM\0" format */
#define DEFAULT_UPDATE_TIME         300 /**< Default tellemetry update time (seconds) */
#define TELEMETRY_QUEUE_MAX_ITEMS   100 /**< Telemetry FIFO Queue Capacity in FRAM */
#define FRAM_RESERVED_BYTES         30 /**< Number of bytes reserved for future fram expansions */
#define APP_MAIN_EVT_QUEUE          10 /**< APP_MAIN Evt Queue allocation */

#define IR_SLOT_COUNT               10      /**< Actions 0 to 9 (OFF, ON, 18°C ​​... 25°C) */
#define IR_SLOT_SIZE_BYTES          3072    /**< 3 KB allocated per slot in FRAM */
#define IR_EVT_DOWNLOAD_IR_RAW      255     /**< Event to Download IR Raw */

// --- LAST_ACTION BITMAP BIT MASKS ---
#define LAST_ACTION_REASON_MASK        (1 << 0)     // Bit 0: 0 = Telemetry, 1 = Scheduling
#define LAST_ACTION_ACTION_MASK        (0x0F << 1)  // Bits 1..4: IR Action Slot (0 to 10)
#define LAST_ACTION_DOWNLOAD_IR_MASK   (1 << 5)     // Bit 5: 1 = Download Requested
#define LAST_ACTION_RAW_SEND_MASK      (1 << 6)     // Bit 6: 1 = Active RAW IR Transmission (CMD 3)

// Bitwise Manipulation Macros
#define GET_LAST_ACTION_REASON(bm)      (((bm) & LAST_ACTION_REASON_MASK) >> 0)
#define GET_LAST_ACTION_ACTION(bm)      (((bm) & LAST_ACTION_ACTION_MASK) >> 1)
#define GET_LAST_ACTION_DOWNLOAD(bm)    (((bm) & LAST_ACTION_DOWNLOAD_IR_MASK) >> 5)
#define GET_LAST_ACTION_RAW_SEND(bm)    (((bm) & LAST_ACTION_RAW_SEND_MASK) >> 6)

#define SET_LAST_ACTION_REASON(bm, val)   ((bm) = ((bm) & ~LAST_ACTION_REASON_MASK) | (((val) & 0x01) << 0))
#define SET_LAST_ACTION_ACTION(bm, val)   ((bm) = ((bm) & ~LAST_ACTION_ACTION_MASK) | (((val) & 0x0F) << 1))
#define SET_LAST_ACTION_DOWNLOAD(bm, val) ((bm) = ((bm) & ~LAST_ACTION_DOWNLOAD_IR_MASK) | (((val) ? 1 : 0) << 5))
#define SET_LAST_ACTION_RAW_SEND(bm, val) ((bm) = ((bm) & ~LAST_ACTION_RAW_SEND_MASK) | (((val) ? 1 : 0) << 6))

typedef uint8_t last_action_t;

/**
 * @brief Enum for readable indexing of IR slots
 */
typedef enum {
    IR_ACTION_NONE        = 0,
    IR_ACTION_POWER_OFF   = 1,
    IR_ACTION_POWER_ON    = 2,
    IR_ACTION_SET_TEMP_18 = 3,
    IR_ACTION_SET_TEMP_19 = 4,
    IR_ACTION_SET_TEMP_20 = 5,
    IR_ACTION_SET_TEMP_21 = 6,
    IR_ACTION_SET_TEMP_22 = 7,
    IR_ACTION_SET_TEMP_23 = 8,
    IR_ACTION_SET_TEMP_24 = 9,
    IR_ACTION_SET_TEMP_25 = 10,
    IR_ACTION_LEARNED_ACK,
    IR_ACTION_DOWNLOAD_ACK
} ir_action_slot_t;

/**
 * @brief Configuration structure saved in FRAM
 */
typedef struct {
    uint16_t telemetry_interval_sec;                    /**< Sending interval in seconds */
    uint8_t reserved[FRAM_RESERVED_BYTES];              /**< Space reserved for future expansions */
} sys_config_t;

/**
 * @brief Structure for the CMD 1 payload (Sync RTC & Telemetry)
 */
typedef struct {
    int cmd_id;
    rtc_date_time_t sync_time_t;
    int telemetry_update;
} cmd1_sync_data_t;

/**
 * @brief Reason for the next system wakeup
 */
typedef enum {
    WAKEUP_REASON_TELEMETRY = 0,
    WAKEUP_REASON_SCHEDULE  = 1
} wakeup_reason_t;

/**
 * @brief Telemetry payload structure
 */
typedef struct {
    int temp;                  
    int umid;                  
    rtc_date_time_t sync_time_t;                
    int rssi;                  
    int battery_mv;            
    last_action_t last_action;
} telemetry_data_t;

/**
 * @brief Wi-Fi credentials structure persisted in FRAM
 */
typedef struct __attribute__((packed)) {
    char ssid[WIFI_SSID_MAX_LEN];
    char password[WIFI_PASS_MAX_LEN];
    uint8_t is_valid;                   /**< Control flag (1 = Valid/Saved Credential, 0 = Empty) */
} wifi_credentials_t;

/**
 * @brief Payload structure for CMD 4 / CMD 5
 */
typedef struct {
    char ssid[WIFI_SSID_MAX_LEN];
    char password[WIFI_PASS_MAX_LEN];
} wifi_prov_payload_t;

/**
 * @brief Structure representing the scheduling payload (CMD 6 / CMD 7)
 */
typedef struct {
    uint8_t schedule_id;                 /**< Schedule ID (0 to 10) */
    uint8_t week_days;                   /**< Bitmask of days + enable bit (LSB) */
    char time[SCHEDULE_TIME_STR_LEN];    /**< "HH:MM" String */
    ir_action_slot_t action;             /**< Action submitted via scheduling */
} schedule_payload_t;

/**
 * @brief Structure that stores the intention/context of the next wakeup in FRAM.
 */
typedef struct __attribute__((packed)) {
    wakeup_reason_t reason;
    uint8_t schedule_id;          /**< Schedule ID (if reason == WAKEUP_REASON_SCHEDULE) */
    last_action_t pending_action; /**< IR action to be triggered upon waking up */
} wakeup_context_t;

/**
 * @brief Ring Buffer control header in FRAM
 */
typedef struct __attribute__((packed)) {
    uint16_t head;  /**< Insertion index (0 to TELEMETRY_QUEUE_MAX_ITEMS - 1) */
    uint16_t tail;  /**< Removal index (0 to TELEMETRY_QUEUE_MAX_ITEMS - 1) */
    uint16_t count; /**< Current number of items (0 to TELEMETRY_QUEUE_MAX_ITEMS) */
    uint16_t magic; /**< Integrity marker (0x5A5A) */
} telemetry_queue_header_t;

#endif // APP_STRUCTS_H