#include "app_power.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "rtc_ht8563.h"
#include "app_storage.h"
#include "board_wifi.h"
#include "board_mqtt.h"

static const char *TAG = "APP_POWER";

#define GPIO_POWER_HOLD_PIN   GPIO_NUM_22
#define GPIO_BTN_MENU_SELECT  GPIO_NUM_5
#define GPIO_BTN_MENU_ENTER   GPIO_NUM_38

#define BUTTON_HOLD_DURATION_MS 1000
#define BUTTON_POLL_INTERVAL_MS 100

/* =========================================================================
 * FUNÇÕES UTILITÁRIAS DE DATA / TEMPO
 * ========================================================================= */

static time_t rtc_to_epoch(const rtc_date_time_t *dt) {
    struct tm t = {
        .tm_sec  = dt->second,
        .tm_min  = dt->minute,
        .tm_hour = dt->hour,
        .tm_mday = dt->day,
        .tm_mon  = dt->month - 1,
        .tm_year = dt->year - 1900,
        .tm_isdst = -1
    };
    return mktime(&t);
}

static void epoch_to_rtc(time_t epoch, rtc_date_time_t *dt) {
    struct tm t;
    localtime_r(&epoch, &t);
    dt->second  = (uint8_t)t.tm_sec;
    dt->minute  = (uint8_t)t.tm_min;
    dt->hour    = (uint8_t)t.tm_hour;
    dt->day     = (uint8_t)t.tm_mday;
    dt->month   = (uint8_t)(t.tm_mon + 1);
    dt->year    = (uint16_t)(t.tm_year + 1900);
    dt->weekday = (uint8_t)t.tm_wday;
}

static int time_str_to_minutes(const char *time_str) {
    if (time_str == NULL) return -1;
    
    int h = -1, m = -1;
    if (sscanf(time_str, "%d:%d", &h, &m) == 2) {
        if (h >= 0 && h < 24 && m >= 0 && m < 60) {
            return h * 60 + m;
        }
    }
    return -1;
}

/* =========================================================================
 * AUXILIARES DE BOOT & HARDWARE
 * ========================================================================= */

static esp_err_t init_boot_gpios(void) {
    gpio_config_t btn_config = {
        .pin_bit_mask = (1ULL << GPIO_BTN_MENU_SELECT) | (1ULL << GPIO_BTN_MENU_ENTER),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    return gpio_config(&btn_config);
}

static bool check_dual_button_hold(void) {
    int elapsed_ms = 0;

    while (elapsed_ms < BUTTON_HOLD_DURATION_MS) {
        bool btn_select_pressed = (gpio_get_level(GPIO_BTN_MENU_SELECT) == 0);
        bool btn_enter_pressed  = (gpio_get_level(GPIO_BTN_MENU_ENTER)  == 0);

        if (btn_select_pressed || btn_enter_pressed) {
            return false;
        }

        vTaskDelay(pdMS_TO_TICKS(BUTTON_POLL_INTERVAL_MS));
        elapsed_ms += BUTTON_POLL_INTERVAL_MS;
    }

    return true;
}

static esp_err_t execute_pending_fram_action(void) {
    wakeup_context_t wakeup_ctx = {0};
    
    esp_err_t err = app_storage_get_wakeup_context(&wakeup_ctx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao ler contexto de wakeup da FRAM: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Contexto recuperado da FRAM - Razao: %d, ID: %d, Action Enum: %d", 
             wakeup_ctx.reason, wakeup_ctx.schedule_id, wakeup_ctx.pending_action);
    
    switch (wakeup_ctx.pending_action) {
        case LAST_ACTION_NONE:
            ESP_LOGI(TAG, "Nenhuma acao IR pendente (Apenas Telemetria).");
            break;
        case LAST_ACTION_LEARNED_ACK:
            ESP_LOGI(TAG, "Acao: LAST_ACTION_LEARNED_ACK");
            break;
        case ACTION_POWER_OFF:
            ESP_LOGI(TAG, "Acao: ACTION_POWER_OFF");
            break;
        case ACTION_POWER_ON:
            ESP_LOGI(TAG, "Acao: ACTION_POWER_ON");
            break;
        case ACTION_SET_TEMP_18:
        case ACTION_SET_TEMP_19:
        case ACTION_SET_TEMP_20:
        case ACTION_SET_TEMP_21:
        case ACTION_SET_TEMP_22:
        case ACTION_SET_TEMP_23:
        case ACTION_SET_TEMP_24:
        case ACTION_SET_TEMP_25:
            ESP_LOGI(TAG, "Acao de temperatura disparada: %d", wakeup_ctx.pending_action);
            break;
        default:
            ESP_LOGW(TAG, "Acao IR desconhecida: %d", wakeup_ctx.pending_action);
            break;
    }

    return ESP_OK;
}

/* =========================================================================
 * INTERFACE PÚBLICA (APP_POWER)
 * ========================================================================= */

esp_err_t app_power_init(void) {
    gpio_config_t pwr_conf = {
        .pin_bit_mask = (1ULL << GPIO_POWER_HOLD_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    esp_err_t ret = gpio_config(&pwr_conf);
    if (ret != ESP_OK) return ret;

    // Mantém circuito energizado
    gpio_set_level(GPIO_POWER_HOLD_PIN, 0);

    return init_boot_gpios();
}

esp_err_t app_power_analyze_boot(boot_event_t *out_event) {
    if (out_event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    bool btn_select_pressed = (gpio_get_level(GPIO_BTN_MENU_SELECT) == 0);
    bool btn_enter_pressed  = (gpio_get_level(GPIO_BTN_MENU_ENTER)  == 0);

    if (!btn_select_pressed && !btn_enter_pressed) {
        ESP_LOGI(TAG, "Dual buttons detected at boot. Checking 2s hold condition...");
        if (check_dual_button_hold()) {
            *out_event = EVENT_WAKEUP_BUTTON_DUAL_HOLD;
        } else {
            *out_event = EVENT_BOOT_POWER_ON;
        }
    } else {
        bool timer_flag = false;  
        bool alarm_flag = false;  

        esp_err_t ret = rtc_ht8563_get_flags(&timer_flag, &alarm_flag);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read HT8563 RTC flags. Defaulting to Power-On Reset.");
            *out_event = EVENT_BOOT_POWER_ON;
            return ret;
        }

        if (timer_flag && alarm_flag) {
            *out_event = EVENT_WAKEUP_SCHEDULE_TELEMETRY_CONFLICT;
        } else if (timer_flag) {
            *out_event = EVENT_WAKEUP_RTC_TIMER;
        } else if (alarm_flag) {
            *out_event = EVENT_WAKEUP_RTC_ALARM;
        } else {
            *out_event = EVENT_BOOT_POWER_ON;
        }
    }

    switch (*out_event) {
        case EVENT_WAKEUP_SCHEDULE_TELEMETRY_CONFLICT:
        case EVENT_WAKEUP_RTC_ALARM:
        case EVENT_WAKEUP_RTC_TIMER:
        case EVENT_WAKEUP_SINGLE_BUTTON:
        case EVENT_BOOT_POWER_ON:
        case EVENT_LOW_BATTERY_SHUTDOWN:
            ESP_LOGI(TAG, "[BOOT CAUSE] Hard Reset / Power-On Reset or RTC detected.");
            execute_pending_fram_action();
            break;

        case EVENT_WAKEUP_BUTTON_DUAL_HOLD:
            ESP_LOGI(TAG, "[BOOT CAUSE] Dual Button Hold (>= 2s) confirmed. Triggering IR Learn/Test Mode.");
            break;

        default:
            ESP_LOGW(TAG, "[BOOT CAUSE] Evento de boot nao mapeado: %d", *out_event);
            break;
    }

    return ESP_OK;
}

esp_err_t app_power_schedule_next_wakeup(void) {
    rtc_date_time_t current_dt;
    esp_err_t err = rtc_ht8563_get_time(&current_dt);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao obter horario atual do RTC HT8563.");
        return err;
    }

    rtc_ht8563_clear_flags();

    uint16_t telemetry_interval_sec = 300;
    if (app_storage_get_telemetry_interval(&telemetry_interval_sec) == ESP_OK)
    {
        ESP_LOGI(TAG, "Intervalo lido da FRAM: %u segundos", telemetry_interval_sec);
    }

    time_t now_epoch = rtc_to_epoch(&current_dt);
    time_t telemetry_target_epoch = now_epoch + telemetry_interval_sec;

    time_t closest_schedule_epoch = 0;
    schedule_payload_t closest_schedule = {0};
    bool found_valid_schedule = false;

    for (uint8_t id = 0; id < MAX_SCHEDULE_ITEMS; id++) {
        schedule_payload_t sched;
        memset(&sched, 0, sizeof(schedule_payload_t));
        
        if (app_storage_get_schedule(id, &sched) != ESP_OK) {
            continue;
        }

        if ((sched.week_days & 0x01) == 0) {
            continue;
        }

        int sched_min = time_str_to_minutes(sched.time);
        if (sched_min < 0) {
            ESP_LOGW(TAG, "Agendamento ID %d com formato de hora invalido: '%s'", id, sched.time);
            continue;
        }

        for (int day_offset = 0; day_offset < 7; day_offset++) {
            time_t candidate_day_epoch = now_epoch + (day_offset * 86400);
            struct tm tm_candidate;
            localtime_r(&candidate_day_epoch, &tm_candidate);

            uint8_t day_bit = (1 << (tm_candidate.tm_wday + 1));

            if (sched.week_days & day_bit) {
                tm_candidate.tm_hour = sched_min / 60;
                tm_candidate.tm_min  = sched_min % 60;
                tm_candidate.tm_sec  = 0;
                tm_candidate.tm_isdst = -1;

                time_t candidate_epoch = mktime(&tm_candidate);

                if (candidate_epoch >= now_epoch) {
                    if (!found_valid_schedule || candidate_epoch < closest_schedule_epoch) {
                        closest_schedule_epoch = candidate_epoch;
                        closest_schedule = sched;
                        found_valid_schedule = true;
                    }
                    break; 
                }
            }
        }
    }

    wakeup_context_t wakeup_ctx = {0};

    if (found_valid_schedule && (closest_schedule_epoch <= telemetry_target_epoch)) {
        rtc_date_time_t target_dt;
        epoch_to_rtc(closest_schedule_epoch, &target_dt);

        rtc_ht8563_clear_flags();
        err = rtc_ht8563_set_alarm(target_dt.hour, target_dt.minute);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "ALARME (AF) PRIORIZADO: Schedule ID=%d para %02d:%02d | Acao IR: %d",
                     closest_schedule.schedule_id, target_dt.hour, target_dt.minute, closest_schedule.action);
        } else {
            ESP_LOGE(TAG, "Erro ao gravar Alarme (AF) no RTC HT8563.");
        }

        wakeup_ctx.reason = WAKEUP_REASON_SCHEDULE;
        wakeup_ctx.schedule_id = closest_schedule.schedule_id;
        wakeup_ctx.pending_action = closest_schedule.action;

    } else {
        rtc_ht8563_clear_flags();
        err = rtc_ht8563_set_timer((uint32_t)telemetry_interval_sec);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "TIMER (TF) PRIORIZADO: Telemetria programada para daqui a %d s.", telemetry_interval_sec);
        } else {
            ESP_LOGE(TAG, "Erro ao gravar Timer (TF) no RTC HT8563.");
        }

        wakeup_ctx.reason = WAKEUP_REASON_TELEMETRY;
        wakeup_ctx.schedule_id = 0xFF;
        wakeup_ctx.pending_action = LAST_ACTION_NONE;
    }

    app_storage_save_wakeup_context(&wakeup_ctx);
    return ESP_OK;
}

void app_power_shutdown(void) {
    app_power_schedule_next_wakeup();

    ESP_LOGI(TAG, "Desligando alimentacao via GPIO_POWER_HOLD_PIN...");
    gpio_set_level(GPIO_POWER_HOLD_PIN, 1);
}