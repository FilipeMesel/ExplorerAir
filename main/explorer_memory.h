/**
 * @file explorer_memory.h
 * @author Filipe Mesel Lobo Costa Cardoso
 * @brief This file contains the function declarations for managing memory (NVS) in the Explorer IR Blaster project.
 * @version 0.1
 * @date 2026-08-31
 * 
 * @copyright Copyright (c) 2026
 * 
 */


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
 * @brief Initialize the memory system (NVS).
 * @return true if initialization was successful, false otherwise.
 */
bool explorer_memory_init(void);

/**
 * @brief Save an IR command to memory by its index.
 * @param cmd_index The index of the command (0 to TOTAL_ACTIONS - 1)
 * @param cmd The IR command to save
 * @return true if the command was saved successfully, false otherwise
 */
bool explorer_memory_save_ir(int cmd_index, const ir_raw_command_t *cmd);

/**
 * @brief Load an IR command from memory by its index.
 * @param cmd_index The index of the command (0 to TOTAL_ACTIONS - 1)
 * @param cmd_out Pointer to the buffer where the loaded command will be stored
 * @return true if the command was loaded successfully, false otherwise
 */
bool explorer_memory_load_ir(int cmd_index, ir_raw_command_t *cmd_out);

/**
 * @brief Save WiFi credentials to memory.
 * @param ssid The WiFi SSID
 * @param password The WiFi password
 * @return true if the credentials were saved successfully, false otherwise
 */
bool explorer_memory_save_wifi_credentials(const char *ssid, const char *password);

/**
 * @brief Load WiFi credentials from memory.
 * @param ssid_out Pointer to the buffer where the loaded SSID will be stored
 * @param max_ssid_len Maximum length of the SSID buffer
 * @param pass_out Pointer to the buffer where the loaded password will be stored
 * @param max_pass_len Maximum length of the password buffer
 * @return true if the credentials were loaded successfully, false otherwise
 */
bool explorer_memory_load_wifi_credentials(char *ssid_out, size_t max_ssid_len, char *pass_out, size_t max_pass_len);

/**
 * @brief Save a schedule by its ID (0 to MAX_SCHEDULES - 1).
 * @param sched Pointer to the schedule to save
 * @return true if the schedule was saved successfully, false otherwise
 */
bool explorer_memory_save_schedule(const schedule_t *sched);

/**
 * @brief Load a schedule by its ID.
 * @param schedule_id The ID of the schedule to load (0 to MAX_SCHEDULES - 1)
 * @param sched_out Pointer to the buffer where the loaded schedule will be stored
 * @return true if the schedule was loaded successfully, false otherwise
 */
bool explorer_memory_load_schedule(uint8_t schedule_id, schedule_t *sched_out);

/**
 * @brief Maps an action string to its corresponding NVS command index.
 * @param action_str The string representation of the action (e.g. "LIGAR", "25 C")
 * @return Returns index 0..TOTAL_ACTIONS-1 if valid, or -1 if invalid/unknown.
 */
int explorer_memory_get_index_by_action(const char *action_str);

#endif // EXPLORER_MEMORY_H