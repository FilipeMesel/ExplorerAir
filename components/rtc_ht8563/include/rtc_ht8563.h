/**
 * @file rtc_ht8563.h
 * @brief Interface for the HT8563 RTC driver.
 */

#ifndef RTC_HT8563_H
#define RTC_HT8563_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define RTC_I2C_ADDR 0x51

typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t weekday;
    uint8_t month;
    uint16_t year;
} rtc_date_time_t;

/**
 * @brief Initialize the RTC driver creating an instance of I2C bus.
 */
esp_err_t rtc_ht8563_init(void);

esp_err_t rtc_ht8563_deinit(void);

/**
 * @brief Read and Write to RTC registers
 */
esp_err_t rtc_ht8563_read_reg(uint8_t reg, uint8_t *data);
esp_err_t rtc_ht8563_write_reg(uint8_t reg, uint8_t data);

/**
 * @brief Functions to convert BCD and control date/time
 */
uint8_t rtc_dec_to_bcd(uint8_t val);
uint8_t rtc_bcd_to_dec(uint8_t val);
esp_err_t rtc_ht8563_set_time(const rtc_date_time_t *dt);
esp_err_t rtc_ht8563_get_time(rtc_date_time_t *dt);

/**
 * @brief Alarme, Timer and Clear Flags AF/TF
 */
esp_err_t rtc_ht8563_clear_flags(void);
esp_err_t rtc_ht8563_set_alarm(uint8_t hour, uint8_t minute);
esp_err_t rtc_ht8563_set_timer(uint8_t seconds);

/**
 * @brief Tests for the RTC functionality. These functions can be called from app_main or other parts of the application.
 *        They are intended for testing and demonstration purposes.
 */
void rtc_ht8563_run_test_log_datetime(void);
void rtc_ht8563_run_test_alarm(uint8_t hour, uint8_t minute);
void rtc_ht8563_run_test_timer(uint8_t seconds);

/**
 * @brief Executes the RTC tests enabled via Kconfig.
 *        Can be called directly from app_main.
 */
void rtc_ht8563_run_configured_tests(void);

#endif // RTC_HT8563_H