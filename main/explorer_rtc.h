#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "explorer_structs.h"

// Inicializa a camada de RTC
void rtc_manager_init(void);

// Sincroniza a hora do sistema via NTP
bool rtc_manager_sync_ntp(void);

// Obtém a hora atual do sistema em struct tm
bool rtc_manager_get_time(struct tm *timeinfo);

// Avalia os agendamentos salvos, executa se necessário e coloca o ESP32 em Deep Sleep
void rtc_manager_process_schedules_and_sleep(void);

// Executa a ação do agendamento (dispara o IR) de acordo com o schedule_t fornecido
void execute_schedule_action(const schedule_t *sched);

#endif // RTC_MANAGER_H