#ifndef EXPLORER_MEMORY_H
#define EXPLORER_MEMORY_H

#include <stdbool.h>
#include <stdint.h>
#include "explorer_structs.h"
#include "ir_handler.h"

#ifdef _INCLUDE_NVS_   
#include "nvs_flash.h"
#include "nvs.h"
#endif

/**
 * @brief Inicializa o subsistema de memória (NVS atualmente, FRAM no futuro).
 * @return true se a inicialização foi bem-sucedida.
 */
bool explorer_memory_init(void);

/**
 * @brief Salva um comando IR de acordo com seu índice numérico.
 */
bool explorer_memory_save_ir(int cmd_index, const ir_raw_command_t *cmd);

/**
 * @brief Carrega um comando IR armazenado pelo seu índice numérico.
 */
bool explorer_memory_load_ir(int cmd_index, ir_raw_command_t *cmd_out);

/**
 * @brief Salva credenciais Wi-Fi.
 */
bool explorer_memory_save_wifi_credentials(const char *ssid, const char *password);

/**
 * @brief Carrega credenciais Wi-Fi.
 */
bool explorer_memory_load_wifi_credentials(char *ssid_out, size_t max_ssid_len, char *pass_out, size_t max_pass_len);

/**
 * @brief Salva um agendamento pelo seu ID (0 a MAX_SCHEDULES - 1).
 */
bool explorer_memory_save_schedule(const schedule_t *sched);

/**
 * @brief Carrega um agendamento pelo seu ID.
 */
bool explorer_memory_load_schedule(uint8_t schedule_id, schedule_t *sched_out);

#endif // EXPLORER_MEMORY_H