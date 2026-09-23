# explorerAirConditioner - Interactive Firmware Testing Suite

An automated and interactive Python-based testing framework designed for testing, validating, and homologating ESP32 firmware running the `explorerAirConditioner` system over MQTT.

This test suite handles end-to-end communication verification, including Wi-Fi reconfiguration, IR code learning/transmission, schedule validation with alarm wakeup triggers, and automated PDF test report generation.

---

## 🛠️ Features

- **Wi-Fi Reconfiguration Test (`CMD 4` / `CMD 5`):** Sends new Wi-Fi credentials to the ESP32, validates acknowledgement payloads, and listens for reconnection on the new network.
- **IR Command Learning (`CMD 3`):** Receives and captures raw IR pulse structures transmitted from the ESP32 local learning routine into memory.
- **Remote IR Transmission (`CMD 8` / `CMD 9`):** Sends raw IR waveforms stored locally back to the ESP32 to populate device memory slots, verifying execution acknowledgements.
- **Schedule & Alarm Validation (`CMD 6` / `CMD 7` / `CMD 0`):** Programs RTC schedule alarms, waits for timed device triggers, and decodes the `last_action` bitmask to verify deep-sleep wakeups, target slots, temperature, humidity, and boot reasons.
- **Automated PDF Report Generation:** Produces an executive test report summarizing session metrics, execution logs, pass/fail status, and raw telemetry history using `ReportLab`.

---

## 📋 Prerequisites

- **Python:** Version `3.8` or higher installed on your system.
- **Network Connection:** Active internet connection to reach the public MQTT broker (`broker.hivemq.com`).

---

## 🚀 Installation & Setup

### 1. Clone or Download the Repository

Download the code files into your local directory. Ensure the python script file (e.g., `test_all_8.py`) is located in your working directory.

### 2. Create a Virtual Environment (Recommended)

Create and activate a virtual environment to isolate project dependencies:

#### Linux / macOS
```bash
python3 -m venv venv
source venv/bin/activate
```

#### Windows (Command Prompt)
```cmd
python -m venv venv
venv\Scripts\activate
```

#### Windows (PowerShell)
```powershell
python -m venv venv
.\venv\Scripts\Activate.ps1
```

### 3. Install Dependencies

Install the necessary Python modules (`paho-mqtt` and `reportlab`):

```bash
pip install paho-mqtt reportlab
```

---

## 💻 Running the Test Suite

Run the main test script using Python:

```bash
python test_all_8.py
```

### Interactive Console Flow

1. **Target MAC Address:** Upon launching, enter your target ESP32 device MAC address (e.g., `E08CFE950F18`).
2. **Control Panel:** Select options `1` through `6` from the main menu:
   - `1`: Test Wi-Fi Network Reconfiguration (`CMD 4` / `CMD 5`)
   - `2`: Test IR Learning (`CMD 3`)
   - `3`: Test Remote IR Write (`CMD 8` / `CMD 9`)
   - `4`: Test Schedules & Dispatches (`CMD 6` / `CMD 7`)
   - `5`: Execute All Tests Sequentially
   - `6`: Exit and Generate PDF Report

---

## 📊 Output Artifacts

Running tests generates local files in the execution directory:

- `relatorio_teste_explorerAir.pdf` — Full visual test report containing test summary tables and telemetry logs.
- `telemetry_log.txt` — Raw JSON payload records for every received telemetry message (`CMD 0`).
- `error_log.txt` — Detailed error trace logs for failed assertions or timeout events.

---

## 🛰️ Protocol & Command Reference

| Command ID (`cmd_id`) | Direction | Description |
| :---: | :---: | :--- |
| **0** | ESP32 -> Broker | Telemetry update containing battery, RTC timestamp, and decoded `last_action` bitmask. |
| **1** | Broker -> ESP32 | Handshake and time-sync response. |
| **2** | Broker -> ESP32 | ACK for received raw IR pulse (`CMD 3`). |
| **3** | ESP32 -> Broker | Raw IR code payload captured during learning mode. |
| **4** | Broker -> ESP32 | Downlink command to set new Wi-Fi credentials (`ssid`, `password`). |
| **5** | ESP32 -> Broker | ACK confirming receipt and validation of new Wi-Fi credentials. |
| **6** | Broker -> ESP32 | Schedule configuration downlink (`schedule_id`, `week_days`, `time`, `action`). |
| **7** | ESP32 -> Broker | ACK confirming schedule creation/storage in memory. |
| **8** | Broker -> ESP32 | Downlink sending raw IR wave array to write into a specific slot. |
| **9** | ESP32 -> Broker | ACK confirming raw IR slot write completion. |