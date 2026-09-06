# JSON Protocol Component (`json_protocol`)

Handles encoding and decoding of JSON messages over MQTT for the `explorerAirConditioner` firmware.

## Encoders (Uplink)

- **CMD 0 (Telemetry):** Encodes sensor values, RTC timestamp, battery voltage, and execution status.
- **CMD 4 (Wi-Fi ACK):** Confirms receipt of new Wi-Fi credentials.
- **CMD 6 (Schedule ACK):** Confirms setting or updating a timer schedule.

## Memory Management Notice

All encoder functions (`json_encode_*`) allocate memory dynamically via `cJSON_PrintUnformatted()`.
**The calling task MUST free the output pointer when done:**

```c
char *json_out = NULL;
if (json_encode_telemetry(&data, &json_out) == ESP_OK) {
    // Send via MQTT or process...
    free(json_out); // Essential to prevent memory leaks
}
```