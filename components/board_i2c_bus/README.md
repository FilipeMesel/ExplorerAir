# Shared I2C Bus Manager (`board_i2c_bus`)

A thread-safe, centralized I2C Bus Manager component designed for ESP-IDF v6.0+ (`driver/i2c_master.h`). This module allows multiple onboard peripherals (such as the HT8563 RTC and SSD1306 OLED display) to safely share a single physical I2C bus without resource contention or data corruption.

## 🌟 Features

- **ESP-IDF v6.0+ Compliant:** Uses the modern `i2c_new_master_bus` driver API instead of legacy drivers.
- **Thread-Safe Architecture:** Built-in FreeRTOS Mutex protection to prevent simultaneous access across different tasks.
- **I2C Scanner:** Integrated bus scanning utility for automatic peripheral discovery during system initialization or debugging.
- **Clean Abstraction:** Encapsulates initialization, device locking, and teardown routines into simple API calls.

---

## 📌 Pin Configuration (Explorer Board Defaults)

| Parameter | Configuration |
| :--- | :--- |
| **SDA Pin** | `GPIO 23` |
| **SCL Pin** | `GPIO 18` |
| **I2C Port** | `I2C_NUM_0` |
| **Glitch Filter** | Enabled (`7` cycles) |
| **Internal Pull-ups** | Enabled |

---

## 🛠️ Integration into CMake

Add `board_i2c_bus` to your project's `components/` directory or include it in your target `CMakeLists.txt`:

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES board_i2c_bus
)
```

## 💻 Usage Example
1. Initialize the Bus and Perform Scan

```c
#include "esp_err.h"
#include "esp_log.h"
#include "board_i2c_bus.h"

static const char *TAG = "MAIN";

void app_main(void) {
    // Initialize the shared I2C bus
    ESP_ERROR_CHECK(board_i2c_bus_init());

    // Run bus scan to detect connected devices (e.g., RTC at 0x51, OLED at 0x3C)
    board_i2c_bus_scan();
}
```

2. Registering a Peripheral Device

```c
i2c_master_bus_handle_t bus_handle = board_i2c_bus_get_handle();

i2c_device_config_t dev_config = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = 0x51, // Example: HT8563 RTC Address
    .scl_speed_hz = 100000,
};

i2c_master_dev_handle_t rtc_dev_handle;
ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, &rtc_dev_handle));
```

3. Safe Multi-Thread Access (Lock/Unlock)

```c
void rtc_read_task(void *pvParameters) {
    while (1) {
        // Acquire bus lock before transaction
        if (board_i2c_bus_lock(100) == ESP_OK) {
            
            // Perform I2C Read/Write transactions here...
            
            // Release bus lock when finished
            board_i2c_bus_unlock();
        } else {
            ESP_LOGE(TAG, "Failed to acquire I2C bus lock within timeout");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

## 📚 API Reference

### `esp_err_t board_i2c_bus_init(void)`
Initializes the master I2C bus and creates the underlying FreeRTOS mutex. Returns `ESP_OK` on success.

### `esp_err_t board_i2c_bus_deinit(void)`
Deinitializes the I2C bus and deletes the mutex, releasing associated memory.

### `i2c_master_bus_handle_t board_i2c_bus_get_handle(void)`
Returns the `i2c_master_bus_handle_t` needed to register device handles via `i2c_master_bus_add_device()`.

### `esp_err_t board_i2c_bus_lock(uint32_t timeout_ms)`
Obtains exclusive thread lock on the bus. Blocks until lock is acquired or `timeout_ms` expires.

### `esp_err_t board_i2c_bus_unlock(void)`
Releases exclusive thread lock on the bus.

### `esp_err_t board_i2c_bus_scan(void)`
Scans I2C addresses from `0x01` to `0x7E` and outputs detected device addresses via `ESP_LOGI`.