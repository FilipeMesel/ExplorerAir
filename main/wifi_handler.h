/**
 * @file wifi_handler.h
 * @author Filipe Mesel Lobo Costa Cardoso
 * @brief This file contains the function declarations for handling Wi-Fi communication in the Explorer IR Blaster project.
 * @version 0.1
 * @date 2026-08-31
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#ifndef WIFI_HANDLER_H
#define WIFI_HANDLER_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Callback function type for reporting Wi-Fi connection status.
 * It checks if the Wi-Fi connection is successful or failed and provides a title and subtitle for display purposes.
 * @param title The title of the status message (e.g., "WIFI OK", "ERRO WIFI")
 * @param subtitle The subtitle of the status message (e.g., "CONECTADO", "FALHA CONEXAO")
 * 
 */
typedef void (*wifi_status_cb_t)(const char *title, const char *subtitle);

/**
 * @brief Initializes the Wi-Fi connection process with a callback for status updates.
 * The function attempts to connect to Wi-Fi networks in the following order:
 *        - 3 Times with the client's network (saved in NVS, if available)
 *        - 3 Times on each of the N Fallback networks defined in config.h
 * 
 * @param cb Callback for status updates to refresh the OLED display in real-time.
 */
void wifi_connect_init(wifi_status_cb_t cb);

/**
 * @brief Returns whether the ESP32 is connected to Wi-Fi and has a valid IP address.
 */
bool wifi_is_connected(void);

/**
 * @brief Returns whether the retry routine has been exhausted (6 attempts without success).
 */
bool wifi_is_failed(void);

/**
 * @brief Get the RSSI (Received Signal Strength Indicator) of the currently connected Wi-Fi network.
 * 
 * @return RSSI value in dBm if connected, or 0 if not connected or if an error occurs.
 */
int8_t wifi_get_rssi(void);

/**
 * @brief Save the Wi-Fi credentials (SSID and password) to non-volatile storage (NVS).
 * 
 * @param ssid Wi-Fi SSID (network name).
 * @param password Wi-Fi password.
 * @return true if saved successfully.
 */
bool wifi_save_credentials(const char *ssid, const char *password);

#endif // WIFI_HANDLER_H