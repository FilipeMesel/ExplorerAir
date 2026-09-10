# Shared SPI Bus Manager (`board_spi_bus`)

A centralized, thread-safe SPI Bus Manager component designed for ESP-IDF v6.0+ (`esp_driver_spi`). This module configures and manages the shared SPI2 host controller on the Explorer platform, allowing peripherals like non-volatile FRAM to communicate efficiently over DMA-enabled SPI.

## 🌟 Features

- **ESP-IDF v6.0+ Compliant:** Integrated with the modular `esp_driver_spi` component structure.
- **DMA Acceleration:** Configured with `SPI_DMA_CH_AUTO` for high-throughput, low-CPU overhead data transfers.
- **Centralized Pin Mapping:** Guarantees consistent hardware line mapping across all SPI slaves on the board.
- **Safe Lifecycle Management:** Provides clean initialization and deinitialization routines to prevent duplicate bus allocations.

---

## 📌 Hardware Pinout

| Signal | ESP32 GPIO | Description |
| :--- | :--- | :--- |
| **SCLK** | `GPIO 15` | SPI Clock Signal |
| **MISO** | `GPIO 34` | Master In Slave Out (Input-only Pin) |
| **MOSI** | `GPIO 2` | Master Out Slave In |
| **Host** | `SPI2_HOST` | Hardware Peripheral Host |

---

## 🛠️ Integration into CMake

Add `board_spi_bus` to your component or project `CMakeLists.txt`:

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES board_spi_bus esp_driver_spi
)
```

## 💻 Usage Example

```c
#include "esp_err.h"
#include "esp_log.h"
#include "board_spi_bus.h"

void app_main(void) {
    // Initialize the shared SPI bus
    ESP_ERROR_CHECK(board_spi_bus_init());

    // Register SPI devices using BOARD_SPI_HOST...
}
```

## 📚 API Reference
`esp_err_t board_spi_bus_init(void)`
Initializes the SPI2_HOST bus with DMA enabled and configured GPIO pins. Returns ESP_OK on success.

`esp_err_t board_spi_bus_deinit(void)`
Frees the allocated SPI bus resources.