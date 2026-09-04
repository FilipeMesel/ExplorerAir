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

## 📐 Component Architecture

The project enforces strict modularity. Components isolate peripheral implementations and expose clean contracts.

```text
explorerAirConditioner/
├── components/
│   ├── board_i2c_bus/        # Centralized I2C Bus Manager (Shared between RTC & Display)
│   ├── board_spi_bus/        # SPI Bus Manager (Dedicated for FRAM Interface)
│   ├── rtc_ht8563/           # HT8563ARZ RTC Driver (BCD, Alarm, Timer, INT Flags)
│   ├── fram_mb85rs512t/      # MB85RS512T SPI FRAM Manager (Memory offsets & Ring Buffer)
│   ├── ir_remote/            # RMT Transceiver for RAW Infrared waveforms
│   ├── display_oled/         # OLED Display Controller & Português UI Menu Flow
│   └── app_telemetry/        # Business Logic Engine (Tasks 0 to 3, Schedule Evaluator)
└── main/
    └── main.c                # System Entry Point & Task Scheduler
```

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
{"cmd_id": 0, "temp": 24, "umid": 60, "rtc": "10:00", "RSSI": "-65", "bat": 3.7, "last_action": 0}
```
`last_action Values:` 
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

### CMD 2 — Learned Command Queue

```json
{"cmd_id": 2, "action": "25 C", "length": 68, "raw_data": [9000, 4500, 560, 560, 560, 1690]}
```

### CMD 4 — WIFI RECEIVED ACK
Receive wifi credentials from mqtt

```json
{"cmd_id": 4, "ssid": "{same wifi as cmd_id 3}", "password": "{same password as cmd_id 3}"}
```

### CMD 6 — Schedule ack
```json
{"cmd_id": 6, "week_days": {same as cmd_id 5}, "time": {same as cmd_id 5}, "action": {same as cmd_id 5}, "status": "OK"}
```
answer to schedule (cmd_id 5)

### CMD 8 — IR Raw ACK: 
Confirms receipt and successful saving of raw IR waveforms into FRAM.

```json
{"cmd_id":8,"status":"OK", "learned": 9}
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
{"cmd_id": 1, "status": "OK", "telemetry_update": 120, "actual_time": "10:42:50", "week_day": 2}
```

`week_day Values:`
0 = Sunday,
1 = Monday,
...,
6 = Saturday.

### CMD 3 — WIFI RECEIVED
Receive wifi credentials from mqtt

```json
{"cmd_id": 3, "ssid": "WIFI", "password": "PASS"}
```

### CMD 5 — Schedule Provisioning

```json
{"cmd_id": 5, "schedule_id": 0, "week_days": 62, "time": "08:00", "action": "SET_TEMP_18"}
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
  "cmd_id": 5,
  "schedule_id": 1,
  "week_days": 3,
  "time": "08:00",
  "action": "SET_TEMP_18"
}
```

### CMD 7 — Set IR Raw Data:

Receives raw IR pulse timings directly from the backend server to overwrite/save to FRAM for a specific action (e.g. "25 C", "ON").

```json
{
  "cmd_id": 7,
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

3. Build the Project:

```bash
idf.py build
```

4. Flash de Project

```bash
idf.py -p <PORT> flash monitor
```
(Replace <PORT> with your serial device name, e.g., COM3 on Windows or /dev/ttyUSB0 on Linux).