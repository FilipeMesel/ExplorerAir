#ifndef APP_STRUCTS_H
#define APP_STRUCTS_H

#include <stdint.h>
#include <stdbool.h>
#include "rtc_ht8563.h"

/**
 * @brief Tamanhos máximos padrão para credenciais Wi-Fi
 */
#define WIFI_SSID_MAX_LEN     32
#define WIFI_PASS_MAX_LEN     64

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

#endif // APP_STRUCTS_H