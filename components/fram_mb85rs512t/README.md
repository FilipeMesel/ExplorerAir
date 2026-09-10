# MB85RS512T FRAM Driver (`fram_mb85rs512t`)

An SPI driver and memory abstraction layer for the **Fujitsu MB85RS512T** 64 KB (512 Kbit) Ferroelectric RAM (FRAM). Built on top of `board_spi_bus` for ESP-IDF v6.0+, this component handles hardware SPI transactions and provides high-level offset abstractions for application storage needs.

## 🌟 Features

- **Direct Hardware SPI Commands:** Implements `WREN`, `WRITE`, `READ`, and `RDSR` opcodes.
- **DMA-Safe Memory Allocations:** Leverages `MALLOC_CAP_DMA` memory for robust, high-speed SPI transactions.
- **Memory Abstraction Layer:** Pre-mapped hardware offsets for system configuration, operating schedules, and a telemetry ring buffer.
- **High Endurance & Speed:** Takes advantage of FRAM's virtually unlimited write endurance ($10^{13}$ cycles) without needing flash page erase delays.

---

## 📌 Hardware Configuration

| Parameter | Configuration |
| :--- | :--- |
| **Chip Select (CS)** | `GPIO 16` |
| **SPI Speed** | `10 MHz` |
| **SPI Mode** | `Mode 0 (CPOL=0, CPHA=0)` |
| **Total Memory Size** | `64 KB (0x0000 - 0xFFFF)` |

---

## 🗺️ Memory Map Architecture

The 64 KB storage space is organized into strict logical sections:

| Offset Range | Size | Allocation |
| :--- | :--- | :--- |
| `0x0000 - 0x07FF` | 2 KB | **System Configurations** |
| `0x0800 - 0x17FF` | 4 KB | **Schedules & IR Pulse Timings** |
| `0x1800 - 0xFFFF` | ~58 KB | **Telemetry Ring Buffer** |

---

## 🛠️ Usage Example

```c
#include "esp_err.h"
#include "esp_log.h"
#include "fram_mb85rs512t.h"

void app_main(void) {
    // Initialize SPI bus and attach FRAM device
    ESP_ERROR_CHECK(fram_init());

    // Write & Read Configuration Data
    const char config_data[] = "NET_SSID=Explorer_AC";
    char read_buffer[32] = {0};

    ESP_ERROR_CHECK(fram_write_config((uint8_t *)config_data, sizeof(config_data)));
    ESP_ERROR_CHECK(fram_read_config((uint8_t *)read_buffer, sizeof(config_data)));

    ESP_LOGI("FRAM", "Read Config: %s", read_buffer);
}
```

### 📚 API Reference

**Low-Level Hardware APIs**
`esp_err_t fram_init(void):` Attaches the FRAM device to the SPI bus.

`esp_err_t fram_read_status(uint8_t *status):` Reads the status register (RDSR).

`esp_err_t fram_write(uint16_t address, const uint8_t *data, size_t len):` Issues WREN and writes raw data to a specific address.

`esp_err_t fram_read(uint16_t address, uint8_t *data, size_t len):` Reads raw data from a specific address.

**High-Level Memory Abstraction APIs**
`fram_write_config(...) / fram_read_config(...):` Read/write to the 2 KB configuration region.

`fram_write_schedules(...) / fram_read_schedules(...):` Read/write to the 4 KB schedule region.

`fram_write_telemetry_ring(...) / fram_read_telemetry_ring(...):` Read/write using relative offsets inside the ~58 KB telemetry ring buffer.