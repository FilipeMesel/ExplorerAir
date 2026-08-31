/**<
 * @file explorer_rtc.h
 * @author Filipe Mesel Lobo Costa Cardoso
 * @brief This file contains the function declarations for handling the RTC (Real-Time Clock) in the Explorer IR Blaster project.
 * @version 0.1
 * @date 2026-08-31
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "explorer_structs.h"

/**
 * @brief Initializes the RTC manager
 * 
 * \return void
 */
void rtc_manager_init(void);

/**
 * @brief Synchronizes the RTC with NTP
 * 
 * @return true 
 * @return false 
 */
bool rtc_manager_sync_ntp(void);

/**
 * @brief Gets the current time from the RTC
 * @param timeinfo Pointer to the struct tm to store the time information
 * @return true if successful, false otherwise
 */
bool rtc_manager_get_time(struct tm *timeinfo);

/**
 * @brief Processes scheduled tasks and puts the ESP32 in Deep Sleep
 * 
 * @return void
 */
void rtc_manager_process_schedules_and_sleep(void);

/**
 * @brief Executes the action of a schedule (triggers the IR) based on the provided schedule_t
 * @param sched Pointer to the schedule_t structure
 * @return void
 */
void execute_schedule_action(const schedule_t *sched);

#endif // RTC_MANAGER_H