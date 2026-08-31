/**
 * @file config.h
 * @author Filipe Mesel Lobo Costa Cardoso
 * @brief This file contains the configuration settings for the Explorer IR Blaster project, 
 * including GPIO pin assignments, network settings, NVS keys, MQTT settings, OLED display settings, 
 * IR settings, and time/schedule settings.
 * @version 0.1
 * @date 2026-08-31
 * 
 * @copyright Copyright (c) 2026
 * 
 */


#ifndef CONFIG_H
#define CONFIG_H

#include "driver/gpio.h"

// --- GPIO SETTINGS ---
#define I2C_SDA_PIN          GPIO_NUM_18    /**I2C SDA GPIO PIN */
#define I2C_SCL_PIN          GPIO_NUM_23    /**I2C SCL GPIO PIN */

#define BUTTON_1_GPIO        GPIO_NUM_5     /**IR Test Button */
#define BUTTON_2_GPIO        GPIO_NUM_34    /**Save Command Button */
#define GPIO_OUTPUT_PIN      GPIO_NUM_22    /**Status LED Button */

#define IR_RECEIVE_PIN       GPIO_NUM_15    /**IR Receive Pin */
#define IR_SEND_PIN          GPIO_NUM_4     /**IR Send Pin */

// --- NETWORK SETTINGS ---
#define WIFI_FALLBACK_SSID    "conectaSenFio"   /**WiFi Fallback SSID */
#define WIFI_FALLBACK_PASS    "123456789"       /**WiFi Fallback Password */

// --- NVS KEYS ---
#define NVS_WIFI_NAMESPACE      "wifi_cfg"      /**WiFi Configuration Namespace */
#define NVS_KEY_SSID            "ssid"          /**WiFi SSID Key */
#define NVS_KEY_PASS            "password"      /**WiFi Password Key */

#define NVS_IR_NAMESPACE        "ir_codes"      /**IR Codes Namespace */

#define NVS_SCHEDULE_NAMESPACE  "schedules"     /**Schedule Configuration Namespace */

// --- MQTT SETTINGS ---
#define MQTT_BROKER_URI       "mqtt://broker.hivemq.com:1883"   /**MQTT Broker URI */

// --- OLED DISPLAY SETTINGS ---
#define I2C_PORT_NUM         I2C_NUM_0  /**I2C Port Number */
#define OLED_I2C_ADDRESS     0x3C       /**OLED I2C Address */

// --- INFRACHTO ---
#define IR_RESOLUTION_HZ     1000000    /**IR Resolution in Hertz */
#define CARRIER_FREQ_HZ      38000      /**Carrier Frequency in Hertz */
#define MAX_BUFFER_SYMBOLS   400        /**Maximum buffer symbols */

// --- TIME SETTINGS  ---
#define MQTT_INTERACTIVITY_TIMEOUT          30  /**MQTT Interactivity Timeout in seconds */
#define MQTT_TX_QUEUE_SEND_TIMOUT_TICKS     100 /**Timeout for sending messages in the MQTT TX queue */

// --- SCHEDULE SETTINGS ---
#define MAX_SCHEDULES 10                        /**Maximum number of schedules */

#endif // CONFIG_H