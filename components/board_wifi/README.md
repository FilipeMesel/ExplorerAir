# Board Wi-Fi Manager (Failover Engine)

ESP-IDF v6.0+ Wi-Fi Manager component supporting dynamic failover retry mechanisms between customer-configured Wi-Fi networks and static backup networks.

---

## Features

* **Primary & Fallback Strategy:** Attempts to connect to a user dynamic network (loaded from FRAM) up to 3 times before sequentially falling back to static/known networks.
* **Event-Driven Architecture:** Emits events via ESP-IDF Event Loop (`BOARD_WIFI_EVENTS`) to keep application logic completely decoupled.
* **Auto Retries:** Configurable max retry attempts per SSID before switching targets.

---

## Usage Example

```c
#include "board_wifi.h"

void app_main(void) {
    ESP_ERROR_CHECK(board_wifi_init());

    // Optional: Load dynamic credentials from FRAM/NVS
    wifi_credential_t dynamic_cred = {
        .ssid = "Home_WiFi",
        .password = "12345678"
    };
    board_wifi_set_dynamic_credential(&dynamic_cred);

    // Start Failover Sequence
    board_wifi_start_failover_connect();
}
```

## Events Emitted

- `BOARD_WIFI_EVENT_CONNECTED:` Successfully obtained IP address.
- `BOARD_WIFI_EVENT_FAILOVER_EXHAUSTED:` Tried all networks up to max retries without success.
- `BOARD_WIFI_EVENT_DISCONNECTED:` Connection was lost.