# OLED Display UI Driver (`display_oled`)

Thread-safe driver and Portuguese UI layout renderer for SSD1306/SH1106 OLED displays on **ESP-IDF v6.0+**, operating over a shared I2C bus manager (`board_i2c_bus`).

---

## 🚀 Features

- **Shared Bus Architecture:** Integrates directly with `board_i2c_bus` using FreeRTOS mutex locks to prevent I2C bus collisions.
- **Enhanced Header Metrics:** Displays firmware version (e.g., `v1.0`) on the top-left and battery percentage (e.g., `100%`) on the top-right using a crisp, high-visibility 5x7 font.
- **Portuguese Native Interface:** Built-in screens designed specifically for AC operation modes (`DESLIGAR`, `LIGAR`, `18 Graus` to `25 Graus`, `ERRO WIFI`).
- **Kconfig Integration:** Toggable hardware validation test runner (`CONFIG_OLED_RUN_TESTS`).

---

## 🛠 Hardware Mapping

| Peripheral | Board Connection |
| :--- | :--- |
| **Interface** | Shared Master I2C (`board_i2c_bus`) |
| **SDA Pin** | `GPIO 23` |
| **SCL Pin** | `GPIO 18` |
| **I2C Address** | `0x3C` (Default) |

---

## 📥 Integration with CMake

Ensure your `CMakeLists.txt` includes `display_oled` and `board_i2c_bus` as dependencies:

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES board_i2c_bus display_oled
)
```

## 💻 Usage Example

```c
#include "esp_err.h"
#include "board_i2c_bus.h"
#include "display_oled.h"

void app_main(void) {
    // 1. Initialize Shared I2C Master Bus
    ESP_ERROR_CHECK(board_i2c_bus_init());

    // 2. Initialize Display Driver
    ESP_ERROR_CHECK(oled_init(OLED_I2C_ADDR_DEFAULT));

    // 3. Set Header Metrics (Battery % & Version)
    oled_set_header_info(95, "v1.0");

    // 4. Render Target Screen State
    oled_show_screen(OLED_SCREEN_IR_LEARN, OLED_CMD_TEMP_22, 0);
}
```

## 🧪 Hardware Self-Test Mode

1. Open menuconfig:

```bash
idf.py menuconfig
```

2. Navigate to:

```text
explorerAirConditioner - Display OLED Configuration
```

3. Enable

```text
[X] Enable OLED Component Hardware Tests (CONFIG_OLED_RUN_TESTS=y)
```

4. Build & Flash:

```bash
idf.py build flash monitor
```

