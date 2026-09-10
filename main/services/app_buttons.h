#ifndef APP_BUTTONS_H
#define APP_BUTTONS_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Inicializa e dispara a Task de leitura de botões para navegação do menu e dual hold.
 * 
 * Esta task gerencia a navegação nas telas de Aprendizado e Teste IR através
 * dos botões SELECT e ENTER, além de monitorar o pressionamento simultâneo
 * de 10 segundos para acionar a saída e o envio de telemetria.
 * 
 * @return esp_err_t ESP_OK em caso de sucesso no disparo da task, ou ESP_FAIL.
 */
esp_err_t app_buttons_start_menu_task(void);

#ifdef __cplusplus
}
#endif

#endif // APP_BUTTONS_H