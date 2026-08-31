#ifndef WIFI_HANDLER_H
#define WIFI_HANDLER_H

#include <stdbool.h>
#include <stdint.h>

// Callback para notificar progresso no display (ex: "WIFI CLIENTE", "TENTATIVA 1/3")
typedef void (*wifi_status_cb_t)(const char *title, const char *subtitle);

/**
 * @brief Inicializa a pilha de rede e gerencia o ciclo de até 6 tentativas de conexão:
 *        - 3 Tentativas na rede do Cliente (salva na NVS)
 *        - 3 Tentativas na rede Fallback ("conectaSenFio")
 * 
 * @param cb Callback de status para atualizar o display OLED em tempo real.
 */
void wifi_connect_init(wifi_status_cb_t cb);

/**
 * @brief Retorna se o ESP32 está conectado ao Wi-Fi e com IP válido.
 */
bool wifi_is_connected(void);

/**
 * @brief Retorna se a rotina de tentativas esgotou (6 tentativas sem sucesso).
 */
bool wifi_is_failed(void);

/**
 * @brief Obtém o valor do RSSI (potência do sinal) da conexão atual em dBm.
 * 
 * @return int8_t Valor do RSSI (ex: -65). Retorna 0 se não estiver conectado.
 */
int8_t wifi_get_rssi(void);

/**
 * @brief Salva as credenciais do Wi-Fi do cliente na NVS.
 * 
 * @param ssid Nome da rede Wi-Fi.
 * @param password Senha da rede Wi-Fi.
 * @return true se salvou com sucesso.
 */
bool wifi_save_credentials(const char *ssid, const char *password);

#endif // WIFI_HANDLER_H