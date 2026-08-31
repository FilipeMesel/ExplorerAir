#include "explorer_rtc.h"
#include "explorer_memory.h"
#include "explorer_structs.h"
#include "ir_handler.h"
#include "display.h"
#include "config.h"

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_sntp.h"

static const char *TAG = "RTC_MANAGER";
static volatile bool g_time_synced = false;

static void time_sync_notification_cb(struct timeval *tv) {
    ESP_LOGI(TAG, "NTP sincronizado com sucesso!");
    g_time_synced = true;
}

void rtc_manager_init(void) {
#ifdef _INCLUDE_EXTERNAL_RTC_
    ESP_LOGI(TAG, "Modo RTC: EXTERNO (HW RTC)");
    // TODO: Inicializar I2C e driver do RTC externo aqui
#else
    ESP_LOGI(TAG, "Modo RTC: INTERNO (ESP32 RTC)");
#endif
    setenv("TZ", "BRT3", 1);
    tzset();
}

bool rtc_manager_sync_ntp(void) {
    ESP_LOGI(TAG, "Iniciando sincronização NTP...");
    display_show_text("SINCRONIZANDO\nHORA (NTP)...");

    g_time_synced = false;

    if (esp_sntp_enabled()) {
        esp_sntp_stop();
    }

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);

    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "a.st1.ntp.br");
    esp_sntp_setservername(2, "time.google.com");

    esp_sntp_init();

    int retry = 0;
    const int retry_count = 15;
    while (!g_time_synced && ++retry <= retry_count) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (g_time_synced) {
#ifdef _INCLUDE_EXTERNAL_RTC_
        // TODO: time_t now; time(&now); rtc_external_set_time(now);
#endif
        return true;
    }

    ESP_LOGE(TAG, "Falha ao sincronizar NTP.");
    return false;
}

bool rtc_manager_get_time(struct tm *timeinfo) {
    time_t now;
    time(&now);
    localtime_r(&now, timeinfo);
    // Retorna false se o tempo do RTC ainda não tiver sido ajustado
    return (timeinfo->tm_year > (2016 - 1900));
}

void execute_schedule_action(const schedule_t *sched) {
    int cmd_index = -1;
    if (strcmp(sched->action, "LIGAR") == 0) cmd_index = 0;
    else if (strcmp(sched->action, "DESLIGAR") == 0) cmd_index = 1;
    else if (strncmp(sched->action, "SET_TEMP_", 9) == 0) {
        int temp = atoi(&sched->action[9]);
        if (temp >= 18 && temp <= 25) cmd_index = 2 + (temp - 18);
    }

    if (cmd_index >= 0) {
        ir_raw_command_t ir_cmd;
        if (explorer_memory_load_ir(cmd_index, &ir_cmd)) {
            ESP_LOGI(TAG, "Disparando IR para ação: %s", sched->action);
            ir_send_command(&ir_cmd);
            char msg[64];
            snprintf(msg, sizeof(msg), "AGEND EXECUTADO:\n%s", sched->action);
            display_show_text(msg);
            vTaskDelay(pdMS_TO_TICKS(1500));
        }
    }
}

void rtc_manager_process_schedules_and_sleep(void) {
    struct tm now_tm;
    if (!rtc_manager_get_time(&now_tm)) {
        ESP_LOGE(TAG, "RTC sem hora válida. Dormindo 3h por segurança.");
        esp_deep_sleep(3 * 3600 * 1000000ULL);
        return;
    }

    int current_minutes = (now_tm.tm_hour * 60) + now_tm.tm_min;
    uint8_t today_bit = (1 << (now_tm.tm_wday + 1)); // Dom=1, Seg=2, ...

    int best_past_id = -1;
    int max_passed_minutes = -1;
    schedule_t best_past_sched;

    int min_future_minutes = 24 * 60 + 1;

    // --- 1. Varre os agendamentos da NVS ---
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        schedule_t sched;
        if (explorer_memory_load_schedule((uint8_t)i, &sched)) {
            bool is_enabled = (sched.week_days & 0x01) != 0;
            bool is_today   = (sched.week_days & today_bit) != 0;

            if (is_enabled && is_today) {
                int sh = 0, sm = 0;
                sscanf(sched.time, "%d:%d", &sh, &sm);
                int sched_minutes = (sh * 60) + sm;

                // Agendamento já passou hoje?
                if (sched_minutes <= current_minutes) {
                    if (sched_minutes > max_passed_minutes) {
                        max_passed_minutes = sched_minutes;
                        best_past_id = i;
                        best_past_sched = sched;
                    }
                } 
                // Agendamento ainda vai acontecer hoje?
                else {
                    if (sched_minutes < min_future_minutes) {
                        min_future_minutes = sched_minutes;
                    }
                }
            }
        }
    }

    // --- 2. Executa o último agendamento que já venceu ---
    if (best_past_id != -1) {
        ESP_LOGI(TAG, "Executando agendamento pendente ID %d", best_past_id);
        execute_schedule_action(&best_past_sched);
    }

    // --- 3. Cálculo do Deep Sleep ---
    uint64_t sleep_seconds = 0;

    if (min_future_minutes <= 24 * 60) {
        // Acorda exatamente no minuto do próximo agendamento (no segundo 00)
        sleep_seconds = (min_future_minutes - current_minutes) * 60 - now_tm.tm_sec;
        
        // Margem de segurança: se o tempo de sono for menor que 5 segundos, dorme pelo menos até o próximo evento
        if (sleep_seconds < 5) {
            sleep_seconds = 5;
        }
        
        ESP_LOGI(TAG, "Próximo alarme hoje às %02d:%02d (Sleep: %llu sec)",
                min_future_minutes / 60, min_future_minutes % 60, sleep_seconds);
    } else {
        // NÃO EXISTE mais alarme hoje (Dia "Vazio" ou sem alarmes futuros)
        int minutes_to_midnight = (24 * 60) - current_minutes;

        if (minutes_to_midnight <= 120) {
            // Faltam 2 horas ou menos para 00:00 -> Dorme até 00:01 do dia seguinte
            sleep_seconds = (minutes_to_midnight + 1) * 60 - now_tm.tm_sec;
            ESP_LOGI(TAG, "Faltam <= 2h para meia-noite. Dormindo até 00:01 do próximo dia (%llu sec)", sleep_seconds);
        } else {
            // Faltam mais de 2 horas para 00:00 -> Acorda a cada 3 horas (10800 seg)
            sleep_seconds = 3 * 3600;
            ESP_LOGI(TAG, "Sem agendamentos próximos. Acordando em 3 horas (%llu sec)", sleep_seconds);
        }
    }

    display_show_text("ENTRANDO EM\nDEEP SLEEP...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    display_power(false);

    ESP_LOGI(TAG, "Entrando em Deep Sleep por %llu segundos.", sleep_seconds);
    esp_deep_sleep(sleep_seconds * 1000000ULL);
}