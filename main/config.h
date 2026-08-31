#ifndef CONFIG_H
#define CONFIG_H

#include "driver/gpio.h"

// --- MAPEAMENTO DE GPIOS (DeviceTree Centralizado) ---
#define I2C_SDA_PIN          GPIO_NUM_18
#define I2C_SCL_PIN          GPIO_NUM_23

#define BUTTON_1_GPIO        GPIO_NUM_5   // Testar IR / Botão 1
#define BUTTON_2_GPIO        GPIO_NUM_34  // Salvar comando / Botão 2
#define GPIO_OUTPUT_PIN      GPIO_NUM_22   // LED de status / GPIO 22

#define IR_RECEIVE_PIN       GPIO_NUM_15  // PINO RX IR
#define IR_SEND_PIN          GPIO_NUM_4   // PINO TX IR

// --- CONFIGURAÇÕES DE REDE
#define WIFI_FALLBACK_SSID    "conectaSenFio"
#define WIFI_FALLBACK_PASS    "123456789"

// --- CHAVES NVS ---
#define NVS_WIFI_NAMESPACE      "wifi_cfg"
#define NVS_KEY_SSID            "ssid"
#define NVS_KEY_PASS            "password"

#define NVS_IR_NAMESPACE        "ir_codes"

#define NVS_SCHEDULE_NAMESPACE  "schedules"

// --- CONFIGURAÇÕES DO MQTT ---
#define MQTT_BROKER_URI       "mqtt://broker.hivemq.com:1883"

// --- CONFIGURAÇÕES DO DISPLAY ---
#define I2C_PORT_NUM         I2C_NUM_0
#define OLED_I2C_ADDRESS     0x3C

// --- INFRAVERMELHO ---
#define IR_RESOLUTION_HZ     1000000
#define CARRIER_FREQ_HZ      38000
#define MAX_BUFFER_SYMBOLS   400

// --- CONFIGURAÇÕES DE TEMPO ---
#define MQTT_INTERACTIVITY_TIMEOUT       30 // Tempo em segundos para considerar o MQTT inativo e encerrar a task de interação
#define MQTT_TX_QUEUE_SEND_TIMOUT_TICKS  100 // Timeout para envio de mensagens na fila MQTT TX

// --- CONFIGURAÇÕES DE SCHEDULE ---
#define MAX_SCHEDULES 10

#endif // CONFIG_H