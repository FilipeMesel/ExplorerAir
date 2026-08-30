#ifndef EXPLORER_PROTOCOL_H
#define EXPLORER_PROTOCOL_H

/**
 * @brief Enumeração para os IDs dos comandos do protocolo
 * 
 */
typedef enum {
    CMD_TELEMETRY        = 0, /**< Comando de telemetria */
    CMD_TELEMETRY_ACK    = 1, /**< Comando de confirmação de telemetria */
    CMD_EXECUTE_IR       = 2, /**< Comando para executar comando IR */
    CMD_EXECUTE_IR_ACK   = 3, /**< Comando de confirmação de execução de comando IR */
    CMD_SET_SLEEP        = 4, /**< Comando para configurar modo de sono */
    CMD_SET_SLEEP_ACK    = 5, /**< Comando de confirmação de configuração de modo de sono */
    CMD_SYNC_LEARNED     = 6, /**< Comando para sincronizar comandos aprendidos */
    CMD_SET_WIFI         = 7, /**< Comando para configurar WiFi */
    CMD_SET_WIFI_ACK     = 8 /**< Comando de confirmação de configuração de WiFi */
} command_id_t;

/**
 * @brief Obtém o nome de exibição de um comando com base em seu ID
 * @param cmd_id ID do comando
 * @return Nome de exibição do comando
 */
static const char* get_cmd_display_name(int cmd_id) {
    switch (cmd_id) {
        case CMD_TELEMETRY:      return "TELEMETRIA";       /**< Comando de telemetria */
        case CMD_TELEMETRY_ACK:  return "TELEMETRIA OK";    /**< Comando de confirmação de telemetria */
        case CMD_EXECUTE_IR:     return "EXECUTANDO IR";    /**< Comando para executar comando IR */
        case CMD_EXECUTE_IR_ACK: return "IR ENVIADO";       /**< Comando de confirmação de execução de comando IR */
        case CMD_SET_SLEEP:      return "CONFIG SLEEP";     /**< Comando para configurar modo de sono */
        case CMD_SET_SLEEP_ACK:  return "SLEEP OK";         /**< Comando de confirmação de configuração de modo de sono */
        case CMD_SYNC_LEARNED:   return "ENVIANDO APREND";  /**< Comando para sincronizar comandos aprendidos */
        case CMD_SET_WIFI:       return "CONFIG WIFI";      /**< Comando para configurar WiFi */
        case CMD_SET_WIFI_ACK:   return "WIFI CONFIG OK";   /**< Comando de confirmação de configuração de WiFi */
        default:                 return "CMD DESCONHECIDO"; /** < Comando desconhecido */
    }
}

#endif