# HT8563ARZ / HYM8563 Real-Time Clock (RTC) Driver

ESP-IDF v6.0+ component for managing the **HT8563ARZ / HYM8563** I2C Real-Time Clock. Designed for ultra-low power IoT applications with shared I2C bus support, BCD time conversions, alarms, and countdown timers.

---

## Features

* **Task 5.1:** Register-level read/write operations via the `board_i2c_bus` shared manager.
* **Task 5.2:** Date and Time structure parsing with automatic Binary-Coded Decimal (BCD) conversion.
* **Task 5.3:** Periodic/scheduled Interrupt Management with Alarm (AF) and Countdown Timer (TF) handling.
* **Power Management:** Deep Sleep wake-up trigger support via the `/INT` open-drain pin.

---

## Hardware Specifications

* **Device:** HT8563ARZ / HYM8563
* **I2C Address:** `0x51` (7-bit)
* **Operating Voltage:** 1.0V – 5.5V
* **Interface:** I2C (Standard Mode 100 kHz)

---

## API Reference

### Initialization & Bus Attachment

```c
#include "rtc_ht8563.h"

// Attaches RTC device handle to the shared board I2C bus
esp_err_t ret = rtc_ht8563_init();

// Removes RTC device handle from the shared bus
rtc_ht8563_deinit();
```

### Reading & Setting Time

```c
rtc_date_time_t dt = {
    .year = 2026,
    .month = 9,
    .day = 4,
    .weekday = 5, // 0 = Sunday, 1 = Monday, ..., 5 = Friday
    .hour = 17,
    .minute = 45,
    .second = 0
};

// Set RTC internal clock
rtc_ht8563_set_time(&dt);

// Read current RTC clock
rtc_date_time_t current_dt;
if (rtc_ht8563_get_time(&current_dt) == ESP_OK) {
    ESP_LOGI("APP", "Current Time: %02d:%02d:%02d", 
             current_dt.hour, current_dt.minute, current_dt.second);
}
```

### Alarm & Timer Interrupt Configuration

```c
// 1. Clear existing Interrupt Flags (AF and TF) and enable Alarm Interrupt (AIE)
rtc_ht8563_clear_flags();

// 2. Set Daily/Hourly Alarm (e.g., trigger at 17:45:00)
rtc_ht8563_set_alarm(17, 45);

// 3. Or Set Countdown Timer Interrupt (e.g., 30 seconds periodic timer)
rtc_ht8563_set_timer(30);
```

### Kconfig Test Battery

Enable tests via menuconfig:

```text
Component config -> RTC HT8563 Configuration
    [*] Enable Date/Time Read Test
    [*] Enable Alarm Interrupt Test
    [*] Enable Timer Countdown Test
```

### Execute from application:

```c
#include "rtc_ht8563.h"

void app_main(void) {
    ESP_ERROR_CHECK(board_i2c_bus_init());
    ESP_ERROR_CHECK(rtc_ht8563_init());

    // Executes test routines selected in Kconfig
    rtc_ht8563_run_configured_tests();
}
```

## Register Map

| Register Address | Name | Description |
| :--- | :--- | :--- |
| `0x00` | `REG_CTRL1` | Control and Status 1 (Stop/Run bits) |
| `0x01` | `REG_CTRL2` | Control and Status 2 (AF, TF flags and AIE, TIE enables) |
| `0x02 – 0x08` | `REG_SEC – REG_YEAR` | BCD Seconds, Minutes, Hours, Days, Weekdays, Months, Years |
| `0x09 – 0x0C` | `REG_MIN_ALARM – REG_WEEK_ALARM` | Alarm Minutes, Hours, Days, and Weekdays |
| `0x0E` | `REG_TIMER_CTRL` | Countdown Timer Control (Enable, Clock Source) |
| `0x0F` | `REG_TIMER_VAL` | Countdown Timer Value |

## Dependencies

- `board_i2c_bus:` Shared I2C Bus Manager component.
- `esp_driver_i2c:` Native ESP-IDF v6.0+ master driver.