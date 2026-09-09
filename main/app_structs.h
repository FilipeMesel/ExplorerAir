#ifndef APP_STRUCTS_H
#define APP_STRUCTS_H

#include <stdint.h>
#include <stdbool.h>
#include "rtc_ht8563.h"

/**
 * @brief Tamanhos máximos padrão para credenciais Wi-Fi
 */
#define WIFI_SSID_MAX_LEN       32
#define WIFI_PASS_MAX_LEN       64
#define MAX_SCHEDULE_ITEMS      11  /**< Agendamentos do ID 0 ao ID 10 */
#define SCHEDULE_TIME_STR_LEN   6   /**< Formato "HH:MM\0" */
#define DEFAULT_UPDATE_TIME     300 /**< Default time to sleep */

/**
 * @brief Estrutura de configuração salva na FRAM
 */
typedef struct {
    uint16_t telemetry_interval_sec; /**< Intervalo de envio em segundos */
    uint8_t reserved[30];            /**< Espaço reservado para expansões futuras */
} sys_config_t;

/**
 * @brief Estrutura para o payload do CMD 1 (Sync RTC & Telemetria)
 */
typedef struct {
    int cmd_id;
    rtc_date_time_t sync_time_t;
    int telemetry_update;
} cmd1_sync_data_t;

/**
 * @brief Enumeration of Last Actions
 */
typedef enum {
    LAST_ACTION_NONE            = 0,
    LAST_ACTION_LEARNED_ACK     = 1,
    ACTION_POWER_OFF            = 2,
    ACTION_POWER_ON             = 3,
    ACTION_SET_TEMP_18          = 4,
    ACTION_SET_TEMP_19          = 5,
    ACTION_SET_TEMP_20          = 6,
    ACTION_SET_TEMP_21          = 7,
    ACTION_SET_TEMP_22          = 8,
    ACTION_SET_TEMP_23          = 9,
    ACTION_SET_TEMP_24          = 10,
    ACTION_SET_TEMP_25          = 11
} last_action_t;

/**
 * @brief Estrutura que guarda a intenção/contexto para o próximo wakeup
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
 * @brief Estrutura de Credenciais Wi-Fi persistida na FRAM
 */
typedef struct __attribute__((packed)) {
    char ssid[WIFI_SSID_MAX_LEN];
    char password[WIFI_PASS_MAX_LEN];
    uint8_t is_valid; /**< Flag de controle (1 = Credencial Válida/Salva, 0 = Vazia) */
} wifi_credentials_t;

/**
 * @brief Estrutura para payload do CMD 4 / CMD 5
 */
typedef struct {
    char ssid[WIFI_SSID_MAX_LEN];
    char password[WIFI_PASS_MAX_LEN];
} wifi_prov_payload_t;

/**
 * @brief Estrutura que representa o payload do Agendamento (CMD 6 / CMD 7)
 */
typedef struct {
    uint8_t schedule_id;                 /**< ID do agendamento (0 a 10) */
    uint8_t week_days;                   /**< Máscara de bits dos dias + enable bit (LSB) */
    char time[SCHEDULE_TIME_STR_LEN];    /**< String no formato "HH:MM" */
    last_action_t action;                /**< Ação enviada no agendamento (ex: ACTION_SET_TEMP_18) */
} schedule_payload_t;

typedef struct __attribute__((packed)) {
    wakeup_reason_t reason;
    uint8_t schedule_id;       /**< ID do agendamento (caso o reason seja WAKEUP_REASON_SCHEDULE) */
    last_action_t pending_action; /**< Ação IR que deve ser disparada ao acordar */
} wakeup_context_t;

#endif // APP_STRUCTS_H