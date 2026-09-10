/**
 * @file app_ui.c
 * @brief High-Level Asynchronous Thread-Safe UI Service Implementation.
 */

#include "services/app_ui.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "APP_UI";

#define UI_QUEUE_LEN        15
#define BATTERY_MAX_MV      4200
#define BATTERY_MIN_MV      3300

static QueueHandle_t s_ui_queue = NULL;
static TaskHandle_t  s_ui_task_handle = NULL;

static uint8_t convert_mv_to_percentage(uint16_t battery_mv) {
    if (battery_mv >= BATTERY_MAX_MV) return 100;
    if (battery_mv <= BATTERY_MIN_MV) return 0;
    return (uint8_t)(((uint32_t)(battery_mv - BATTERY_MIN_MV) * 100) / (BATTERY_MAX_MV - BATTERY_MIN_MV));
}

/**
 * @brief Task consumidora dedicada exclusivamente a desenhar na tela
 */
static void app_ui_task(void *pvParameters) {
    ui_msg_t msg;
    ESP_LOGI(TAG, "Task de UI pronta e aguardando comandos...");

    while (1) {
        if (xQueueReceive(s_ui_queue, &msg, portMAX_DELAY) == pdTRUE) {
            switch (msg.type) {
                case UI_CMD_UPDATE_HEADER: {
                    uint8_t pct = convert_mv_to_percentage(msg.data.header.battery_mv);
                    oled_set_header_info(pct, msg.data.header.fw_version);
                    break;
                }
                case UI_CMD_SHOW_BOOT:
                    oled_show_screen(OLED_SCREEN_BOOT, OLED_CMD_POWER_OFF, 0);
                    break;

                case UI_CMD_SHOW_MAIN_MENU:
                    oled_show_screen(OLED_SCREEN_MENU_MAIN, OLED_CMD_POWER_OFF, msg.data.main_menu.selected_index);
                    break;

                case UI_CMD_SHOW_IR_LEARN:
                    oled_show_screen(OLED_SCREEN_IR_LEARN, msg.data.ir_step.action, 0);
                    break;

                case UI_CMD_SHOW_IR_TEST:
                    oled_show_screen(OLED_SCREEN_IR_TEST, msg.data.ir_step.action, 0);
                    break;

                case UI_CMD_SHOW_WIFI_ERROR:
                    oled_show_screen(OLED_SCREEN_WIFI_ERROR, OLED_CMD_POWER_OFF, 0);
                    break;

                case UI_CMD_SHOW_MESSAGE:
                    oled_show_message(msg.data.message.line1, msg.data.message.line2);
                    if (msg.data.message.display_ms > 0) {
                        vTaskDelay(pdMS_TO_TICKS(msg.data.message.display_ms));
                    }
                    break;

                case UI_CMD_SHOW_SLEEP_PREP:
                    oled_show_screen(OLED_SCREEN_SLEEP_PREP, OLED_CMD_POWER_OFF, 0);
                    break;

                case UI_CMD_CLEAR:
                    oled_clear();
                    break;

                default:
                    break;
            }
        }
    }
}

/* =========================================================================
 * INICIALIZAÇÃO & DEINIT
 * ========================================================================= */

esp_err_t app_ui_init(void) {
    ESP_LOGI(TAG, "Inicializando Serviço AsSEncrono de UI...");
    
    esp_err_t ret = oled_init(OLED_I2C_ADDR_DEFAULT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar o driver OLED: %s", esp_err_to_name(ret));
        return ret;
    }

    s_ui_queue = xQueueCreate(UI_QUEUE_LEN, sizeof(ui_msg_t));
    if (s_ui_queue == NULL) {
        ESP_LOGE(TAG, "Falha ao criar a fila de UI!");
        return ESP_ERR_NO_MEM;
    }

    BaseType_t task_ret = xTaskCreate(app_ui_task, "app_ui_task", 3072, NULL, 4, &s_ui_task_handle);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Falha ao criar a Task da UI!");
        vQueueDelete(s_ui_queue);
        s_ui_queue = NULL;
        return ESP_FAIL;
    }

    app_ui_post_header(4200, "v1.0");
    return ESP_OK;
}

esp_err_t app_ui_deinit(void) {
    if (s_ui_task_handle != NULL) {
        vTaskDelete(s_ui_task_handle);
        s_ui_task_handle = NULL;
    }
    if (s_ui_queue != NULL) {
        vQueueDelete(s_ui_queue);
        s_ui_queue = NULL;
    }
    oled_clear();
    return oled_deinit();
}

/* =========================================================================
 * IMPLEMENTAÇÃO DAS APIS POST (THREAD-SAFE)
 * ========================================================================= */

static esp_err_t send_to_ui_queue(const ui_msg_t *msg) {
    if (s_ui_queue == NULL) return ESP_ERR_INVALID_STATE;
    if (xQueueSend(s_ui_queue, msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Fila de UI cheia! Mensagem descartada.");
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

esp_err_t app_ui_post_header(uint16_t battery_mv, const char *fw_version) {
    ui_msg_t msg = { .type = UI_CMD_UPDATE_HEADER };
    msg.data.header.battery_mv = battery_mv;
    snprintf(msg.data.header.fw_version, sizeof(msg.data.header.fw_version), "%s", fw_version ? fw_version : "v1.0");
    return send_to_ui_queue(&msg);
}

esp_err_t app_ui_post_booting(void) {
    ui_msg_t msg = { .type = UI_CMD_SHOW_BOOT };
    return send_to_ui_queue(&msg);
}

esp_err_t app_ui_post_main_menu(uint8_t selected_index) {
    ui_msg_t msg = { .type = UI_CMD_SHOW_MAIN_MENU };
    msg.data.main_menu.selected_index = selected_index;
    return send_to_ui_queue(&msg);
}

esp_err_t app_ui_post_wifi_error(void) {
    ui_msg_t msg = { .type = UI_CMD_SHOW_WIFI_ERROR };
    return send_to_ui_queue(&msg);
}

esp_err_t app_ui_post_message(const char *line1, const char *line2, uint32_t display_ms) {
    ui_msg_t msg = { .type = UI_CMD_SHOW_MESSAGE };
    snprintf(msg.data.message.line1, sizeof(msg.data.message.line1), "%s", line1 ? line1 : "");
    snprintf(msg.data.message.line2, sizeof(msg.data.message.line2), "%s", line2 ? line2 : "");
    msg.data.message.display_ms = display_ms;
    return send_to_ui_queue(&msg);
}

esp_err_t app_ui_post_sleep_prep(void) {
    ui_msg_t msg = { .type = UI_CMD_SHOW_SLEEP_PREP };
    return send_to_ui_queue(&msg);
}

esp_err_t app_ui_post_clear(void) {
    ui_msg_t msg = { .type = UI_CMD_CLEAR };
    return send_to_ui_queue(&msg);
}