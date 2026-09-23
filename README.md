# EXPLORER IR BLASTER

A high-reliability, ultra-low-power IoT firmware for smart air conditioner control and telemetry monitoring. Powered by an ESP32 microcontroller running **ESP-IDF v6.0.1**, this system operates in a cyclic power-managed scheme using an external Real-Time Clock (RTC) and an auto-hold power circuit to achieve microampere-level current consumption in idle/sleep states.

---

## 📌 Features

* **Ultra-Low-Power Architecture:**
  * Uses a hardware Power-Hold latch circuit (`ESP_REG_GPIO 22`) and an external RTC (`HT8563ARZ`) to cut off system power completely.
  * Wakeup triggers supported: Daily RTC Alarm, RTC Countdown Timer, and Manual Power Button (`GPIO 38`).

* **Hardware Component Decoupling (Shared Bus Pattern):**
  * Modular component architecture with isolated bus managers (`board_i2c_bus` and `board_spi_bus`) preventing resource collisions across drivers.

* **Non-Volatile Data Storage (SPI FRAM):**
  * **MB85RS512T (64 KB):** High-endurance non-volatile memory storing network profiles, 11 weekly schedules (ID 0–10), raw IR signal pulse buffers, and a 500-entry offline telemetry ring buffer.

* **Infrared Engine (RMT Driver):**
  * High-precision carrier generation for sending and capturing raw IR pulse trains (20°C–25°C, ON, OFF) across multiple HVAC brands.

* **MQTT Protocol & Telemetry Synchronization:**
  * Real-time MQTT telemetry updates, scheduling management, time-syncing, and Wi-Fi credential provisioning with failover support across N fallback networks.

* **Guided Learning & Manual Test Interface:**
  * On-board UI/UX via SSD1306/SH1106 OLED display and two tactile buttons (`GPIO 5` & `GPIO 38`) for capturing and testing raw IR commands on-site without server connectivity.

---

## 🛠️ Hardware Specifications & Pinout

| Component | Function / Signal | ESP32 GPIO / Interface | Notes |
| :--- | :--- | :--- | :--- |
| **Power Hold** | System Power Latch | `GPIO 22` | High = Hold Power, Low = Power Cutoff |
| **Buttons** | Select / Confirm IR | `GPIO 5` | Internal Pull-Up |
| | Power / Enter / Wake | `GPIO 38` | Hardware Interruption / Wakeup Pin |
| **Status LEDs** | Red LED | `GPIO 4` | System Error / Warning Indicator |
| | Blue LED | `GPIO 33` | Wi-Fi / MQTT Activity Indicator |
| **I2C Bus Manager** | `I2C_SDA` | `GPIO 23` | Shared Bus (RTC + OLED Display) |
| | `I2C_SCL` | `GPIO 18` | Shared Bus Clock Line |
| **SPI Bus Manager** | `FRAM_CLK` | `GPIO 15` | SPI Clock |
| | `FRAM_MISO` | `GPIO 34` | SPI Master Input (Input-Only Pin) |
| | `FRAM_MOSI` | `GPIO 2` | SPI Master Output |
| | `FRAM_CS` | `GPIO 16` | FRAM Chip Select |
| **Infrared (RMT)** | `IR_TX` | `GPIO 25` | Carrier Transmission (NPN Transistor Driver) |
| | `IR_RX` | `GPIO 35` | Demodulated Receiver (TSOP/VS1838B, Input-Only) |
| **Sensors & ADC** | Ambient Temp/Humidity | `GPIO 27` | OneWire Protocol (DHT22 / SHT3x) |
| | Battery Level Read | `GPIO 32` | ADC1 Channel 4 (1:2 Voltage Divider) |

---

## 📐 Component Architecture & Directory Structure

The project enforces strict modularity. Peripherals and low-level board drivers live in `components/`, while business services, messaging protocols, event definitions, and system execution logic are cleanly organized inside `main/`.

```text
explorerAirConditioner/
├── bootloader_components/   # Custom early-stage bootloader hooks and configurations
├── components/              # Modular Hardware Peripheral Drivers
│   ├── board_i2c_bus/       # Centralized I2C Bus Manager (Shared: RTC & Display)
│   ├── board_spi_bus/       # SPI Bus Manager (Dedicated for FRAM Interface)
│   ├── rtc_ht8563/          # HT8563ARZ RTC Driver (BCD, Alarm, Timer, INT Flags)
│   ├── fram_mb85rs512t/     # MB85RS512T SPI FRAM Low-Level Driver (Pure Byte Read/Write)
│   ├── ir_remote/           # RMT Transceiver for RAW Infrared waveforms
│   └── display_oled/        # OLED Display Controller & Português UI Menu Flow
├── test_script/             # Folder with python test script suit
└── main/                    # Main Application Domain Logic
    ├── protocol/            # Messaging Encoders & Serialization
    │   ├── json_protocol.c  # Lightweight JSON Encoders & Parsers for MQTT Payloads
    │   └── json_protocol.h  # Header for JSON protocol definitions
    ├── services/            # System Business Services
    │   ├── app_comms.c      # Networking Subsystem (Wi-Fi Manager & MQTT Client Engine)
    │   ├── app_comms.h      # Header for Wi-Fi and MQTT interfaces
    │   ├── app_power.c      # Power Management (Hold Pin, HT8563 Calculators & Shutdown)
    │   ├── app_power.h      # Header for Power Control & Deep Sleep interfaces
    │   ├── app_ui.h         # Header for Oled User Interface
    │   ├── app_ui.c         # Oled User Interface Service based in msg mechanism
    │   ├── app_buttons.h    # Header User interactive menu of learning IR and testing IR
    │   ├── app_buttons.c    # ser interactive menu of learning IR and testing IR
    │   ├── app_storage.c    # High-level FRAM Abstraction (Schedules, IR Slots, Telemetry Buffer)
    │   └── app_storage.h    # Storage Offsets & FRAM Service API
    ├── app_events.h         # Global Inter-Task Event Definitions & Event Queue Structs
    ├── app_structs.h        # Compact Data Structs & Packed Types (Zero-heap Overhead)
    ├── CMakeLists.txt       # Main Module Build Script
    ├── idf_component.yml    # ESP Component Manager Dependencies
    ├── Kconfig.projbuild    # Project Configuration Menu Settings
    └── main.c               # Entry Point, FSM Task (g_app_event_queue) & Timer Management
```

**Key Architectural Separation**

* Low-Level Hardware Drivers (components/): The fram_mb85rs512t driver is strictly responsible for SPI raw byte communication, free from protocol or business logic.
* Data Definitions (main/app_structs.h): Defines packed C structures (e.g., schedule_item_t, telemetry_data_t) to optimize RAM footprint and ensure zero heap overhead during JSON parsing.
* Storage Abstraction (main/app_storage): Maps the 64KB FRAM memory regions, handling ring-buffer indexes, schedule slots, and IR raw wave storage.

## Operational Sequence & Execution Lifecycle

```text
stateDiagram-v2
    [*] --> Task0_BootReason

    state Task0_BootReason {
        CheckButton: Power Button Pressed (GPIO 38)?
        CheckRTC: Read AF / TF Flags from HT8563
    }

    Task0_BootReason --> IR_LearningMode: Both Buttons held for 2 sec
    Task0_BootReason --> Task1_ReadFRAM: Normal Boot Flow

    state IR_LearningMode {
        Aprender: Learn IR Sequence -> Save FRAM -> Connect MQTT -> Send CMD 2
        Testar: Test IR Signal -> 25 min Timeout Protection
    }

    state Task1_ReadFRAM {
        ExecutePending: Execute Last Action (Emit IR waveform if pending action != 0)
    }

    Task1_ReadFRAM --> Task2_WiFiMQTT

    state Task2_WiFiMQTT {
        ConnectWiFi: Try Client Wi-Fi (3x) -> Try Fallback Networks (3x)
        SendMQTT: Transmit Stored Offline Logs + Send Initial Telemetry (CMD 0)
        ReceiveCMD1: Synchronize RTC Clock & Update Telemetry Period
    }

    Task2_WiFiMQTT --> Task3_CalculateWakeup

    state Task3_CalculateWakeup {
        EvaluateSchedules: Evaluate (Current_Time + Telemetry_Period) vs Next Enabled Schedule
        ProgramRTC: Configure RTC Countdown Timer or Daily Alarm
        PersistState: Write Next Action ID to FRAM
    }

    Task3_CalculateWakeup --> PowerDown
    PowerDown: Clear GPIO 22 (Power-Hold) / Cut System Power
```

## 📡 MQTT Message Specifications

Device Topics are structured dynamically using the unique Wi-Fi MAC Address:

* **Device Publish Topic (TX):** `explorer/ac/{MAC_ADDRESS}/tx`
* **Device Subscribe Topic (RX):** `explorer/ac/{MAC_ADDRESS}/rx`

## 📤 Outbound Messages (Device -> Server)

### CMD 0 — Initial Telemetry

```json
{"cmd_id":0,"temp":24,"umid":58,"rtc":"14:30:30","day":8,"month":9,"year":2026,"weekday":2,"rssi":-65,"bat":3.70,"last_action":10}
```
`last_action is a bitmap that should have the followed Values:` 
0 = Telemetry,
1 = OFF, 
2 = ON, 
3 = 18°C, 
4 = 19°C, 
5 = 20°C, 
6 = 21°C, 
7 = 22°C, 
8 = 23°C, 
9 = 24°C, 
10 = 25°C.
11 = IR Commands was learned

### CMD 3 — Learned Command Queue

```json
{"cmd_id": 3, "action": 9, "length": 68, "raw_data": [9000, 4500, 560, 560, 560, 1690]}
```
Where:
0 = OFF, 
1 = ON, 
2 = 18°C, 
3 = 19°C, 
4 = 20°C, 
5 = 21°C, 
6 = 22°C, 
7 = 23°C, 
8 = 24°C, 
9 = 25°C.

### CMD 5 — WIFI RECEIVED ACK
Receive wifi credentials from mqtt

```json
{"cmd_id": 5, "ssid": "{same wifi as cmd_id 3}", "password": "{same password as cmd_id 3}"}
```

### CMD 7 — Schedule ack
```json
{"cmd_id": 7, "week_days": {same as cmd_id 5}, "time": {same as cmd_id 5}, "action": {same as cmd_id 5}, "status": "OK"}
```
answer to schedule (cmd_id 5)

### CMD 9 — IR Raw ACK: 
Confirms receipt and successful saving of raw IR waveforms into FRAM.

```json
{"cmd_id":9,"status":"OK", "learned": 9}
```

Where:
- 0 = OFF
- 1 = ON
- 2 = 18
- 3 = 19
- 4 = 20
- 5 = 21
- 6 = 22
- 7 = 23
- 8 = 24
- 9 = 25

## 📥 Inbound Messages (Server -> Device)

### CMD 1 — Telemetry Acknowledgment / Time Sync

```json
{ "cmd_id": 1, "year": 2026, "month": 9, "day": 8, "hour": 21, "min": 38, "sec": 0,  "weekday": 2, "telemetry_update": 10 }
```

`week_day Values:`
0 = Sunday,
1 = Monday,
...,
6 = Saturday.

### CMD 2 — Get Learned Command Queue Transmission
The platform request for a learned IR to store in a online database

```json
{"cmd_id": 2, "action": 9}
```

Where:
- 0 = OFF
- 1 = ON
- 2 = 18
- 3 = 19
- 4 = 20
- 5 = 21
- 6 = 22
- 7 = 23
- 8 = 24
- 9 = 25

### CMD 4 — WIFI RECEIVED
Receive wifi credentials from mqtt

```json
{"cmd_id": 4, "ssid": "WIFI", "password": "PASS"}
```

### CMD 6 — Schedule Provisioning

```json
{"cmd_id": 6, "schedule_id": 0, "week_days": 62, "time": "08:00", "action": "SET_TEMP_18"}
```

`week_days Bitmask:` Bit 0 = Enable Flag. Bits 1–7 = Sun–Sat (e.g., 62 = 0011 1110 in binary -> Enabled for Mon, Tue, Wed, Thu, Fri).

Where 62 = 0011 1110 in binary

from right to left:

the first 0 means disable (the schedule will be saved but not yet active)

the second 1 means Sunday

1 means Monday... the last 0 (the last bit, on the left) represents Saturday

schedule_id is a schedule ID. There will be only 10 schedules, ranging from ID 0 to ID 10.

Other example: enable and Sunday -> 3 = 011

```json
{
  "cmd_id": 6,
  "schedule_id": 1,
  "week_days": 3,
  "time": "08:00",
  "action": "SET_TEMP_18"
}
```

### CMD 8 — Set IR Raw Data:

Receives raw IR pulse timings directly from the backend server to overwrite/save to FRAM for a specific action (e.g. "25 C", "ON").

```json
{
  "cmd_id": 8,
  "action": 9,
  "length": 128,
  "raw_data": [4402,4377,537,1608,536,536,536,1607,536,1608,535,536,537,535,535,1609,535,537,535,536,535,1608,536,536,535,537,534,1609,536,1607,535,537,536,1608,535,1609,535,536,536,1608,535,1608,535,1607,536,1609,536,1608,535,1608,536,536,535,1608,535,538,533,537,535,537,535,536,535,537,535,536,535,1608,535,1609,535,536,534,538,533,538,535,537,535,536,535,536,535,537,535,536,535,1608,535,1608,535,1608,536,1608,535,1608,536,1608,535,5190,4375,4379,534,1609,535,536,535,1609,535,1609,534,538,534,537,534,1610,534,537,535,537,532,1610,535,537,535,536,535,1608]
}
```
Where:
- 0 = OFF
- 1 = ON
- 2 = 18
- 3 = 19
- 4 = 20
- 5 = 21
- 6 = 22
- 7 = 23
- 8 = 24
- 9 = 25

### Protocol Exchange Summary Table (`cmd_id`)

| Trigger / Source | Sent Message | Origin `cmd_id` | Origin Description | Expected Response | Response `cmd_id` | Response Description |
| :--- | :--- | :---: | :--- | :--- | :---: | :--- |
| **ESP32 → Platform** | `Initial Telemetry` | **CMD 0** | Sends device metrics (temp, humidity, battery, RTC, `last_action`). | `Telemetry ACK / Time Sync` | **CMD 1** | Acknowledges telemetry receipt and synchronizes RTC date/time on ESP32. |
| **Platform → ESP32** | `Get Learned Command` | **CMD 2** | Platform requests a locally learned IR pulse waveform from memory. | `Learned Command Queue` | **CMD 3** | Transmits raw IR pulse array (`raw_data`) for the requested slot. |
| **Platform → ESP32** | `WIFI RECEIVED` | **CMD 4** | Transmits new Wi-Fi credentials (`ssid`, `password`) to the ESP32. | `WIFI RECEIVED ACK` | **CMD 5** | ESP32 ACK echoing back the received Wi-Fi credentials for confirmation. |
| **Platform → ESP32** | `Schedule Provisioning`| **CMD 6** | Configures a schedule alarm (`schedule_id`, `week_days`, `time`, `action`). | `Schedule ACK` | **CMD 7** | ESP32 ACK confirming schedule creation/storage with "OK" status. |
| **Platform → ESP32** | `Set IR Raw Data` | **CMD 8** | Overwrites/writes raw IR waveforms (`raw_data`) into a specific FRAM slot. | `IR Raw ACK` | **CMD 9** | ESP32 ACK confirming raw IR waveform saved into FRAM memory successfully. |

---

### Communication Pairs Reference

- **`CMD 0` ➔ `CMD 1`**: Device telemetry update triggers platform time synchronization.
- **`CMD 2` ➔ `CMD 3`**: Platform requests local IR command learned on ESP32 to store in database.
- **`CMD 4` ➔ `CMD 5`**: Remote Wi-Fi credentials reconfiguration and confirmation handshake.
- **`CMD 6` ➔ `CMD 7`**: Alarm schedule provision saved into RTC/FRAM and acknowledged.
- **`CMD 8` ➔ `CMD 9`**: Backend remote raw IR slot population and FRAM write confirmation.

---

## System Architecture: `last_action` Bitmask & IR Command Sync Flow

The system represents the `last_action` field in telemetry payloads using an 8-bit bitmask structure. This optimized design enables real-time status decoding for deep-sleep wakeups, scheduling triggers, and active IR data streams.

### Bitmask Field Structure (`uint8_t`)

| Bit Offset | Field Name | Description | Values |
| :--- | :--- | :--- | :--- |
| **Bit 0** | Wakeup Reason | Trigger source of the system wakeup. | `0`: Telemetry Timer<br>`1`: Schedule Alarm |
| **Bits 1..4** | Action Slot ID | Target IR action associated with the schedule trigger. | `0`: NONE<br>`1`: POWER_OFF<br>`2`: POWER_ON<br>`3..10`: Temp 18°C to 25°C |
| **Bit 5** | Download Pending | Local menu user request to download IR codes. | `0`: Inactive<br>`1`: Download Request Pending |
| **Bit 6** | RAW IR Stream | Active raw IR transmission state. | `0`: Idle<br>`1`: CMD 3 RAW IR Transmission Active |
| **Bit 7** | Reserved | Reserved for future firmware expansion. | `0` |

### `CMD_ID_GET_IR_LEARNED` (cmd_id: 2) & Persistence Sequence Flow

```text
+----------------+                +-------------------+                +--------------------+
|  MQTT Broker   |                | ESP32 IR Blaster  |                | FRAM Persistence   |
+-------+--------+                +---------+---------+                +---------+----------+
        |                                   |                                    |
        |--- CMD 2 (action: 0..8) --------->|                                    |
        |                                   |--- Save BIT 6 = 1 (RAW Active) --->|
        |                                   |--- Reset 30s Shutdown Timer ------>|
        |<-- CMD 3 (RAW IR Slot Payload) ---|                                    |
        |                                   |                                    |
```

1. **Downlink Request Handling (`cmd_id: 2`)**:
   - The device receives a request containing the targeted `action` index (0 through 8).
   - `action` index map to internal FRAM slots (Slots 1 to 9).
2. **Timer & State Preservation**:
   - Bit 6 of `last_action` is asserted and persisted to FRAM.
   - The 30-second system power-off timer is continuously reset upon every valid `cmd_id: 2` packet received to guarantee complete sequence transfers without unexpected low-power cutoffs.
3. **Uplink Response (`cmd_id: 3`)**:
   - The ESP32 retrieves the pulse sequence stored in FRAM for the targeted slot and returns it through a `cmd_id: 3` JSON response.

## IR Downloading Workflow (Download Mode)

When the **DOWNLOAD** option is selected in the local menu (`MENU_STATE_IR_DOWNLOAD`), the ESP32 exits the learning mode and triggers the automated IR data synchronization sequence with the cloud platform.

### Workflow Sequence

1. **Triggering Download Mode:**
   - The user selects the **DOWNLOAD** option via the device menu.
   - The ESP32 connects to Wi-Fi/MQTT and publishes an initial ACK command indicating readiness for IR downloading:

   **Device Response (CMD 9):**
   ```json
   {
     "cmd_id": 9,
     "action_idx": 255,
     "status": "SUCCESS"
   }
   ```
  
2. Platform Sync Loop:
  - Upon receiving cmd_id: 9 with action_idx: 255, the cloud platform must initiate the sequential download process.
  - The platform sends CMD 8 (CMD_ID_SET_IR_RAW_DATA) starting with the first command slot (e.g., IR Power Off at index 0).
  - For every CMD 8 received, the ESP32 stores the IR raw payload into FRAM and replies with its respective confirmation ACK (CMD 9 with the matching action_idx).
  - The platform waits for each CMD 9 acknowledgment before sending the next IR command slot until all required IR commands are downloaded.


## 🚀 Building & Flashing

**Prerequisites**

- ESP-IDF v6.0.1 configured in environment.
- CMake & Ninja build tools.

**Step-by-Step Instructions**

1. Clone Repository:

```bash
git clone [https://github.com/FilipeMesel/ExplorerAir.git](https://github.com/FilipeMesel/ExplorerAir.git)
cd ExplorerAir
```

2. Set Target Microcontroller:

```bash
idf.py set-target esp32
```

3. Add dependency:

```bash
idf.py add-dependency "espressif/mqtt"
idf.py add-dependency "espressif/cjson^1.7.18"
```

4. Build the Project:

```bash
idf.py build
```

5. Flash de Project

```bash
idf.py -p <PORT> bootloader-flash flash monitor
```
(Replace <PORT> with your serial device name, e.g., COM3 on Windows or /dev/ttyUSB0 on Linux).