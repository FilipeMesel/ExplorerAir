# ESP32 IR Blaster & Learning System

An ESP-IDF (v5.x / v6.x) firmware for ESP32 designed to capture, test, save, and transmit Infrared (IR) commands. The device features an SSD1306 OLED display for user feedback, dual push-button controls, and automatic MQTT reporting over Wi-Fi once the learning cycle completes.

---

## 🛠 Features

* **IR Learning & Emulation:** Receives raw IR pulse streams via RMT RX and transmits them via RMT TX modulated at 38 kHz.
* **Finite State Machine (FSM):** Non-blocking state management preventing CPU execution locks.
* **Anti-Flicker Display Handling:** Smart string tracking prevents redundant I2C writes to the SSD1306 display.
* **Thread-Safe I2C:** Uses FreeRTOS Mutex synchronization for safe OLED access across tasks.
* **Automated Data Upload:** Aggregates captured IR timing sequences into JSON format and publishes them to an MQTT broker (`broker.hivemq.com`).

---

## 📌 Pinout & Hardware Configuration

All GPIO allocations are centralized in `main/config.h` for straightforward maintenance:

| Periph / Module | ESP32 GPIO | Description |
| :--- | :--- | :--- |
| **I2C SDA** | `GPIO 18` | OLED Display Data |
| **I2C SCL** | `GPIO 23` | OLED Display Clock |
| **Button 1** | `GPIO 5` | Test captured IR command |
| **Button 2** | `GPIO 34` | Save command & advance step |
| **IR Receiver** | `GPIO 15` | TSOP Receiver input |
| **IR Transmitter** | `GPIO 4` | IR LED active driver |

---

## 📦 Project Architecture

```text
├── CMakeLists.txt              # Root CMake configuration
└── main/
    ├── CMakeLists.txt          # Component CMake configuration
    ├── idf_component.yml       # ESP Component Manager dependencies
    ├── config.h                # Central pinout definition
    ├── display.h / display.c   # SSD1306 I2C display driver
    ├── ir_handler.h / ir_handler.c # RMT RX/TX IR signal handlers
    ├── mqtt_handler.h / mqtt_handler.c # Wi-Fi and MQTT client logic
    └── explorer_irblaster.c    # Main application & FSM task
```

---

## ⚙️ External Dependencies
In modern ESP-IDF releases (v5.x and v6.x), official network protocol components like MQTT are hosted on the ESP Component Manager registry instead of being pre-packaged into the core IDF repository.

To resolve the mqtt component dependency, the project requires adding espressif/mqtt via the component manager:

```bash
idf.py add-dependency "espressif/mqtt"
```

This creates a main/idf_component.yml file:

```yaml
dependencies:
  espressif/mqtt: "^1.0.0"
```

The same happens for Json:

```bash
idf.py add-dependency "espressif/cjson"
```

Component Registration (main/CMakeLists.txt)
Ensure your CMake configuration references the modular driver components (esp_driver_gpio, esp_driver_rmt, esp_driver_i2c) and the registry MQTT package (espressif__mqtt):

```text
idf_component_register(
    SRCS 
        "explorer_irblaster.c"
        "display.c"
        "ir_handler.c"
        "mqtt_handler.c"
    INCLUDE_DIRS 
        "."
    REQUIRES 
        esp_driver_gpio
        esp_driver_rmt
        esp_driver_i2c
        nvs_flash
        esp_wifi
        esp_event
        espressif__mqtt
)
```

---

## Communication Protocol & MQTT Commands

The project uses an asynchronous communication protocol over MQTT for telemetry transmission, IR control command reception, and local learning synchronization.

### Operational Flow

1. **Telemetry & Listening Cycle:** Upon connecting to the network, the device sends an initial telemetry payload (`CMD 0`) containing sensor data and the last executed action status. After receiving confirmation from the server (`CMD 1`), the ESP32 enters an active listening state to dynamically process IR emission requests (`CMD 2`), sleep/standby instructions (`CMD 4`), or future commands.
2. **Guided Learning Mode:** Triggered via physical button (**GPIO 05**), the network stack is paused to isolate IR signal acquisition. The process follows a fixed sequence:
   - `OFF` $\rightarrow$ `ON` $\rightarrow$ `18°C` $\rightarrow$ ... $\rightarrow$ `25°C`
   - The **GPIO 34** button allows testing the emission of the captured IR code at any step.
   - Upon completing the `25°C` step and advancing with **GPIO 05**, the device exits learning mode, reconnects to Wi-Fi/MQTT, sends `CMD 0`, and sequentially transmits all learned IR codes via **`CMD 6`**.

---

### JSON Command Specifications

#### 📤 Commands Sent by Device (TX)

* **Command 0 — Initial Telemetry / Reboot**
  Sent upon connecting to the MQTT broker to report current sensor readings and the last action performed.
```json
{
"comando": 0,
"temp": 24,
"umid": 60,
"rtc": "10:00",
"last_action": "LEARNED_FULL_SET"
}
```

* **Command 3 — IR Emission Acknowledgment (ACK)**
Sent to the server following the successful execution of an IR emission request (CMD 2).

```json
  {
  "comando": 3,
  "status": "OK"
  }
```

* **Command 5 — Sleep Request Acknowledgment (ACK)**
Sent to the server confirming the scheduled standby duration (CMD 4).

```json
{
  "comando": 5,
  "status": "OK"
}
```

* **Command 6 — Learned Command Queue Transmission**
Fired sequentially after completing the Guided Learning Mode to register the raw IR pulse data for each captured button on the server.

```json
{
  "comando": 6,
  "action": "25 C",
  "length": 68,
  "raw_data": [9000, 4500, 560, 560, 560, 1690]
}
```

#### 📥 Commands Received from Server (RX)

* **Command 1 — Telemetry Acknowledgment / Handshake**
Sent by the server in response to CMD 0 to confirm telemetry data has been received and processed.

```json
{
  "comando": 1,
  "status": "OK"
}
```

* **Command 2 — IR Emission Order**
Instructs the ESP32 to transmit the infrared signal corresponding to the provided action label.

```json
{
  "comando": 2,
  "action": "SET_TEMP_25"
}
```

* **Command 4 — Sleep / Standby Request**
Specifies a standby duration (in seconds) for the device before reopening the connection cycle with a new CMD 0.

```json
{
  "comando": 4,
  "sleep": 10
}
```

---

## 🚀 How to Build and Flash
Set up the target board:

```bash
idf.py set-target esp32
```

Configure Wi-Fi Credentials:
Update your network details inside main/config.h:

```bash
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASS "YOUR_WIFI_PASSWORD"
```

Build the project:

```bash
idf.py -p COMX build flash monitor
```
---

## 🕹 Usage Flow
Power-On: Display stays turned off by default.

Start Learning: Press Button 1 and Button 2 simultaneously. The display powers on and prompts LEARNING....

Capture Sequence:

Step labels prompt on display (DESLIGAR, LIGAR, 18*C ... 25*C).

Aim remote control at the receiver. Display updates to IR OK when captured.

Press Button 1 (GPIO 5) to test/re-transmit the command via the IR LED (TESTE OK).

Press Button 2 (GPIO 34) to confirm saving (OK) and advance to the next step.

Publish Data: Upon completing 25*C, the system connects to Wi-Fi, formats all timing sequences into JSON, uploads to explorer/command/{MAC}, displays Enviado OK, and powers off the display.

## 📋 Project Roadmap

| Category | Activity | Status |
| :--- | :--- | :--- |
| **IR Capture & Transmission** | Raw IR signal transmission via RMT (38 kHz) | Completed |
| **IR Capture & Transmission** | Raw IR pulse capture via RMT RX | Completed |
| **Interface & Peripherals** | SSD1306 OLED display driver via I2C with FreeRTOS Mutex | Completed |
| **Interface & Peripherals** | Physical button handling and debounce (GPIO 5 & GPIO 34) | Completed |
| **Communication & Protocol** | Wi-Fi connection management (NVS & Fallback) and MQTT client | Completed |
| **Communication & Protocol** | cJSON parser and serializer for Telemetry, ACKs, and Sync (CMD 0 to 8) | Completed |
| **Business Logic** | Finite State Machine (FSM) for Guided Learning Mode | Completed |
| **Power Management** | Deep/Light Sleep cycle implementation based on received command interval | Pending |
| **Power Management** | Timer-based wakeup routine to periodically check schedule queue | Pending |
| **External Memory (FRAM)** | I2C/SPI driver integration for external FRAM storage | Pending |
| **Scheduler & RTC** | Schedule management engine for timed AC unit activation | Pending |
| **Scheduler & RTC** | Missed schedule detection algorithm to handle missed runtime windows | Pending |
| **Offline Logging (FRAM)** | Local log retention in FRAM during Wi-Fi connection loss | Pending |
| **Offline Logging (FRAM)** | Log synchronization and flush logic upon Wi-Fi re-establishment | Pending |