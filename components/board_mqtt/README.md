# Board MQTT Client (Dynamic MAC Topics)

ESP-IDF v6.0+ MQTT Client Wrapper featuring dynamic topic generation using the ESP32 Wi-Fi Station MAC address.

---

## Features

* **Dynamic Topic Generation:** Formats topics using the hardware MAC Address automatically:
  * **UPLINK (TX):** `explorerIRBlaster/{MAC}/UPLINK`
  * **DOWNLINK (RX):** `explorerIRBlaster/{MAC}/DOWNLINK`
* **Event-Driven:** Posts data payloads and connection status through system event loops (`BOARD_MQTT_EVENTS`).

---

## Usage Example

```c
#include "board_mqtt.h"

void app_main(void) {
    ESP_ERROR_CHECK(board_mqtt_init("mqtt://broker.your-mqtt.com:your-port"));
    ESP_ERROR_CHECK(board_mqtt_start());

    // Publish to UPLINK topic
    board_mqtt_publish_uplink("{\"cmd_id\":0,\"status\":\"OK\"}", 1);
}
```

## Events Emitted
- `BOARD_MQTT_EVENT_CONNECTED:` Connected and subscribed to Downlink topic.
- `BOARD_MQTT_EVENT_DISCONNECTED:` Client disconnected from broker.
- `BOARD_MQTT_EVENT_DATA_RECEIVED:` Incoming payload on Downlink topic (board_mqtt_data_t).