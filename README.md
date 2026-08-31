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
├── CMakeLists.txt                  # Root CMake configuration
└── main/
    ├── CMakeLists.txt              # Component CMake configuration
    ├── idf_component.yml           # ESP Component Manager dependencies
    ├── config.h                    # Central pinout definition
    ├── explorer_structs.h          # Data structures (schedule_t, ir_raw_command_t)
    ├── display.h / display.c       # SSD1306 OLED display driver (I2C)
    ├── ir_handler.h / ir_handler.c # RMT RX/TX IR signal handlers
    ├── mqtt_handler.h / mqtt_handler.c # Wi-Fi, MQTT client, and JSON parser logic
    ├── explorer_memory.h / .c      # NVS storage abstraction (IR and Schedules)
    ├── explorer_rtc.h / .c         # Time management (NTP, RTC, Scheduler, Deep Sleep)
    └── explorer_irblaster.c        # Main application entry point (app_main) & FSM
```

### Component Registration

```text
idf_component_register(
    SRCS 
        "explorer_irblaster.c"
        "display.c"
        "ir_handler.c"
        "mqtt_handler.c"
        "explorer_memory.c"
        "explorer_rtc.c"
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
        espressif__cjson
)
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

## ⏰ Scheduler Logic & Deep Sleep Management
The explorer_rtc module coordinates schedule execution and manages the ESP32 sleep duration.

```text
                        [ Power On / Reset ]
                                 │
                 ┌───────────────┴───────────────┐
                 ▼                               ▼
       [ Buttons 1 & 2 Pressed? ]     [ Normal Boot Sequence ]
                 │                               │
       (YES)     │                               ▼
  ┌──────────────┘                     [ Connect Wi-Fi & Sync NTP ]
  ▼                                              │
[ Enter Guided Learning Mode ]          ┌────────┴────────┐
  │                                     ▼                 ▼
  ├─► Wait IR Pulse per Step       (Success)          (Failure)
  ├─► Button 1: Test Command            │                 │
  ├─► Button 2: Save to NVS             └────────┬────────┘
  │                                              ▼
  └─► Complete 25°C Step                [ Read Schedules from NVS ]
         │                                       │
         ▼                         ┌─────────────┴─────────────┐
  [ Reconnect Wi-Fi & MQTT ]       ▼                           ▼
         │               [ Missed Schedules Today? ]   [ Next Future Schedule ]
         ▼                  └─► Execute latest missed     └─► Calculate sleep
  [ Bulk Upload CMD 6 Queue ]                                 down to 00s
         │                                 │                   │
         └─────────────────────────────────┴─────────┬─────────┘
                                                     ▼
                                          [ Configure Deep Sleep ]
                                                     │
                                                     ▼
                                            [ Enter Deep Sleep ]
```

1. Dual-Path Operational Logic

* Guided IR Learning Path (Manual Trigger): If Button 1 (GPIO 5) and Button 2 (GPIO 34) are pressed simultaneously during boot, the network stack is held back to isolate hardware resources. The system sequentially captures, tests, and saves raw IR pulse timing arrays to NVS across all states (OFF $\rightarrow$ ON $\rightarrow$ 18°C ... 25°C). Once completed, it connects to Wi-Fi/MQTT and bulk-transmits all saved sequences via CMD 6.
* Automated Scheduler Path (Normal Boot): The device syncs time via NTP (falling back to local RTC if offline), queries NVS for scheduled tasks, processes pending actions, and sets a Deep Sleep timer.

2. Selection & Execution of Pending Schedules

* The system converts current time into minutes passed today (current_minutes = hr * 60 + min).
* It iterates through stored NVS schedules filtered by the active day of the week (week_days).
* If schedules expired earlier today (sched_minutes <= current_minutes), the system identifies the most recent missed schedule and executes it via execute_schedule_action().

3. Deep Sleep Calculation
To prevent missing back-to-back schedules in consecutive minutes:

* Upcoming Alarm Today: The sleep timer wakes the chip at second 00 of the target minute:

$$\text{sleep\_seconds} = (\text{min\_future\_minutes} - \text{current\_minutes}) \times 60 - \text{now\_tm.tm\_sec}$$

A safety threshold enforces a minimum sleep time of 5 seconds.

* No Upcoming Alarms Remaining Today:

If $\le 2\text{ hours}$ remain until midnight, the device sleeps until 00:01 of the following day.
If $> 2\text{ hours}$ remain, the device enters a periodic sleep cycle waking up every 3 hours to re-evaluate schedule queues.

---

## 📡 Communication Protocol & MQTT Commands

Operational Flow

1. Initial Telemetry (CMD 0): Upon boot/connection, the device transmits sensor values, local RTC time, and last executed action state.
2. Server Handshake (CMD 1): Server acknowledges receipt of initial telemetry.
3. Remote Commands: The server can trigger immediate IR emissions (CMD 2), order manual standby mode (CMD 4), or manage schedules (CMD 7 / CMD 8).
4. Guided Learning Mode: Triggered via physical buttons, pauses network connections to acquire IR sequences:
DESLIGAR $\rightarrow$ LIGAR $\rightarrow$ 18°C $\rightarrow$ ... $\rightarrow$ 25°C.

Once complete, captured IR data arrays are uploaded via CMD 6.

### JSON Command Specifications
#### 📤 Commands Sent by Device (TX)

* CMD 0 — [**IMPLEMENTED**] Initial Telemetry / Boot: Reports telemetry, RTC status, and last executed action.

```json
{"cmd_id":0,"temp":24,"umid":60,"rtc":"10:00","RSSI":-32}
```

* CMD 3 — [**IMPLEMENTED**] IR Emission ACK: Confirms completion of requested IR signal transmission (CMD 2).

```json
{"cmd_id":3,"status":"OK","action":"SET_TEMP_25"}
```

* CMD 5 — [**NOT IMPLEMENTED**] Sleep Request ACK: Confirms scheduled standby duration received from server (CMD 4).

* CMD 6 — [**IMPLEMENTED**] Learned IR Transmission: Sequential upload of raw_data arrays captured during learning mode.

```json
{
  "cmd_id":9,
  "action":"25 C",
  "length":128,
  "raw_data":[4402,4377,537,1608,536,536,536,1607,536,1608,535,536,537,535,535,1609,535,537,535,536,535,1608,536,536,535,537,534,1609,536,1607,535,537,536,1608,535,1609,535,536,536,1608,535,1608,535,1607,536,1609,536,1608,535,1608,536,536,535,1608,535,538,533,537,535,537,535,536,535,537,535,536,535,1608,535,1609,535,536,534,538,533,538,535,537,535,536,535,536,535,537,535,536,535,1608,535,1608,535,1608,536,1608,535,1608,536,1608,535,5190,4375,4379,534,1609,535,536,535,1609,535,1609,534,538,534,537,534,1610,534,537,535,537,532,1610,535,537,535,536,535,1608
  ]
}
```

* CMD 8 — [**IMPLEMENTED**] Wi-Fi credentials saved: Confirms that the new Wi-Fi SSID and Password was saved.

```json
{"cmd_id":8,"status":"OK"}
```

* CMD 10 — [**IMPLEMENTED**] New schedule received: Confirms that the new schedule was saved.

```json
{"cmd_id":10,"status":"OK","schedule_id":1}
```

#### 📥 Commands Received from Server (RX)

* CMD 1 — [**IMPLEMENTED**] Telemetry ACK / Handshake: Server acknowledgement for CMD 0.

```json
{ "cmd_id": 1, "status": "OK"}
```

* CMD 2 — [**IMPLEMENTED**] IR Emission Request: Triggers transmission of specified IR action (e.g., "SET_TEMP_25").

```json
{"cmd_id": 2,"action": "SET_TEMP_25"}
```

* CMD 4 — [**NOT IMPLEMENTED**] Standby Request: Mandates standby duration in seconds before reconnecting.

* CMD 7 — [**IMPLEMENTED**] Wi-Fi Credentials: Receive a new WiFi SSID and Password.

```json
{"cmd_id": 7, "ssid": "VIVOFIBRA-56ED_EXT", "password": "72233756ED"}
```

* CMD 9 — [**IMPLEMENTED**] New schedule: Receive a new new schedule.

```json
{"cmd_id": 9, "schedule_id": 1, "week_days": 5, "time": "10:05", "action": "SET_TEMP_18"}
```

---

## 🚀 How to Build and Flash
Set up the target board:

```bash
idf.py set-target esp32
```

Add the dependencies:

```bash
idf.py add-dependency "espressif/mqtt"
```

```bash
idf.py add-dependency "espressif/cjson"
```

Build the project:

```bash
idf.py -p COMX build flash monitor
```
---

## 🕹 Usage Flow

Here is the complete Usage Flow section written in English Markdown (.md), covering the end-to-end mechanism of the system across its operational states:

### 1. Boot-Up & Dual-Path Selection

Upon power-on or wake-up from Deep Sleep, the device checks the state of the physical push buttons:

```text
                  ┌───────────────────────────────┐
                  │      System Power-On / Boot    │
                  └───────────────┬───────────────┘
                                  │
                   Is Button 1 (GPIO 5) AND
                 Button 2 (GPIO 34) pressed?
                                  │
                  ├─── (YES) ───► [ Guided Learning Mode ]
                  │
                  └─── (NO)  ───► [ Automated Scheduler Path ]
```

### 2. Guided IR Learning Mode (Manual Trigger)

If both Button 1 and Button 2 are held during startup, the Wi-Fi stack is kept disabled to ensure non-blocking IR capture, and the system powers on the SSD1306 OLED display:

1. Step-by-Step Acquisition Sequence: The FSM cycles sequentially through required AC remote commands:

$$\text{DESLIGAR} \longrightarrow \text{LIGAR} \longrightarrow \text{18°C} \longrightarrow \text{19°C} \longrightarrow \dots \longrightarrow \text{25°C}$$

2. IR Capture: Aim the original AC remote control at the TSOP receiver (GPIO 15). The OLED display confirms successful pulse acquisition (IR OK).

3. Command Testing: Press Button 1 (GPIO 5) to emit the captured raw pulse stream via the IR LED driver (GPIO 4) to verify responsiveness with the AC unit (TESTE OK).

4. Saving & Step Advance: Press Button 2 (GPIO 34) to write the timing array to NVS and proceed to the next state (OK).

5. Bulk Upload & Exit: Once the final state (25°C) is saved, the device connects to Wi-Fi/MQTT, bulk-uploads all raw IR timing arrays using CMD 6, displays Enviado OK, powers off the OLED display, and proceeds to the Deep Sleep evaluation sequence.

### 3. Automated Scheduler Path (Normal Boot)
When booted normally without holding both buttons:

1. Time Sync & Fallback: The ESP32 connects to Wi-Fi and queries NTP servers (pool.ntp.org, a.st1.ntp.br, time.google.com) to synchronize its local clock (timezone set to BRT3). If Wi-Fi fails, it falls back to local RTC time.

3. Telemetry Handshake: Upon establishing an MQTT connection, the device transmits an initial telemetry report (CMD 0) containing sensor readings and current RTC time, expecting a server handshake (CMD 1).

3. Pending Schedule Evaluation:

* Reads stored schedules from NVS (schedule_t structs).
* Filters events for the current day of the week.
* If any schedule expired earlier in the day (sched_minutes <= current_minutes), the system selects the most recent missed task and immediately fires the corresponding IR action via execute_schedule_action().

4. Server Command Execution: The device processes inbound real-time MQTT orders such as immediate IR firing (CMD 2), Wi-Fi credential updates (CMD 7), or schedule configuration (CMD 9).

### 4. Dynamic Deep Sleep Management
After completing network exchanges and executing pending tasks, the device prepares for power saving:

1. Calculating Next Event:Target Event Today:

* Computes the exact delay to wake the processor at second 00 of the next target schedule minute:
$$\text{sleep\_seconds} = (\text{min\_future\_minutes} - \text{current\_minutes}) \times 60 - \text{now\_tm.tm\_sec}$$
* No Further Events Today: If less than 2 hours remain until midnight, it sleeps until 00:01 of the next day. Otherwise, it enters a 3-hour periodic sleep cycle.

2. Power Down: The OLED display is explicitly powered down (display_power(false)), and the chip enters esp_deep_sleep(). Upon waking up, the cycle repeats from Step 1.

---

## 📋 Project Roadmap

| Category | Activity | Status |
| :--- | :--- | :--- |
| **IR Capture & Transmission** | Raw IR signal transmission via RMT (38 kHz) | Completed |
| **IR Capture & Transmission** | Raw IR pulse capture via RMT RX | Completed |
| **IR Capture & Transmission** | Receive raw IR waveform via MQTT and save directly to NVS Flash | Pending |
| **Interface & Peripherals** | SSD1306 OLED display driver via I2C with FreeRTOS Mutex | Completed |
| **Interface & Peripherals** | Physical button handling and debounce (GPIO 5 & GPIO 34) | Completed |
| **Communication & Protocol** | Wi-Fi management with 6 connection retries (3 per AP) and display error fallback | Completed |
| **Communication & Protocol** | MQTT client integration and inter-task communication via Queue to main FSM | Completed |
| **Communication & Protocol** | Sequential learned IR payload bulk upload via MQTT with 3 retry attempts | Completed |
| **Communication & Protocol** | Initial telemetry transmission and server handshake protocol (CMD 0 to 10) | Completed |
| **Communication & Protocol** | Remote Wi-Fi credentials update via MQTT and persistent saving in NVS | Completed |
| **Communication & Protocol** | Real-time IR emission command processing via MQTT | Completed |
| **Business Logic** | Finite State Machine (FSM) for Guided IR Learning Mode | Completed |
| **Scheduler & RTC** | MQTT Schedule reception, parsing, and non-volatile storage | Completed |
| **Scheduler & RTC** | Missed schedule execution logic on boot/wakeup using NTP time | Completed |
| **Scheduler & RTC** | External HW RTC driver integration and sync in `explorer_rtc` | Pending |
| **Power Management** | Deep Sleep wake-up logic with schedule-aware timer routines | Completed |
| **Power Management / Telemetry** | Battery circuit reading module for voltage monitoring (ADC & sub-tasks) | Pending |
| **Sensors & Environment** | PT100 circuit acquisition module for precise temperature/humidity telemetry | Pending |
| **External Memory (FRAM)** | Hardware integration and refactoring of `explorer_memory` driver to use FRAM | Pending |
| **Offline Logging (FRAM)** | Local log retention in FRAM during Wi-Fi connection loss | Pending |
| **Offline Logging (FRAM)** | Log synchronization and flush logic upon Wi-Fi re-establishment | Pending |

**Note:** Everything needs to be validated