/**
 * @file app_ui.c
 * @brief High-Level Asynchronous Thread-Safe UI Service & Layout Business Logic.
 */

#include "app_ui.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "display_oled.h"

static const char *TAG = "APP_UI";

#define UI_QUEUE_LEN        15
#define BATTERY_MAX_MV      4200
#define BATTERY_MIN_MV      3300

static QueueHandle_t s_ui_queue = NULL;
static TaskHandle_t  s_ui_task_handle = NULL;

static uint8_t s_battery_pct = 100;
static char s_fw_version[DISPLAY_FW_VERSION_LENGTH] = "v1.0";

/* Localized Strings (Business Logic UI mappings) */
static const char *s_action_strings[UI_IR_CMD_MAX] = {
    [UI_IR_CMD_POWER_OFF] = "DESLIGAR",
    [UI_IR_CMD_POWER_ON]  = "LIGAR",
    [UI_IR_CMD_TEMP_18]   = "18 C",
    [UI_IR_CMD_TEMP_19]   = "19 C",
    [UI_IR_CMD_TEMP_20]   = "20 C",
    [UI_IR_CMD_TEMP_21]   = "21 C",
    [UI_IR_CMD_TEMP_22]   = "22 C",
    [UI_IR_CMD_TEMP_23]   = "23 C",
    [UI_IR_CMD_TEMP_24]   = "24 C",
    [UI_IR_CMD_TEMP_25]   = "25 C"
};

static uint8_t convert_mv_to_percentage(uint16_t battery_mv) {
    if (battery_mv >= BATTERY_MAX_MV) return 100;
    if (battery_mv <= BATTERY_MIN_MV) return 0;
    return (uint8_t)(((uint32_t)(battery_mv - BATTERY_MIN_MV) * 100) / (BATTERY_MAX_MV - BATTERY_MIN_MV));
}

static void render_header(void) {
    // Top Left: Firmware Version
    oled_draw_string_5x7(0, 0, s_fw_version);

    // Top Right: Battery Percentage
    char bat_str[8];
    snprintf(bat_str, sizeof(bat_str), "%d%%", s_battery_pct);
    int bat_x = OLED_WIDTH - (strlen(bat_str) * 6);
    oled_draw_string_5x7(bat_x > 0 ? bat_x : 0, 0, bat_str);

    // Header Divider Line (y = 9)
    oled_draw_hline(0, 9, OLED_WIDTH, true);
}

static void render_centered_string(int y, const char *str) {
    if (!str) return;
    int x = (OLED_WIDTH - (strlen(str) * 6)) / 2;
    oled_draw_string_5x7(x > 0 ? x : 0, y, str);
}

/**
 * @brief UI Execution Task consuming and rendering queued layout commands.
 */
static void app_ui_task(void *pvParameters) {
    ui_msg_t msg;
    ESP_LOGI(TAG, "UI Task running and waiting for events...");

    while (1) {
        if (xQueueReceive(s_ui_queue, &msg, portMAX_DELAY) == pdTRUE) {
            switch (msg.type) {
                case UI_CMD_UPDATE_HEADER:
                    s_battery_pct = convert_mv_to_percentage(msg.data.header.battery_mv);
                    strncpy(s_fw_version, msg.data.header.fw_version, sizeof(s_fw_version) - 1);
                    break;

                case UI_CMD_SHOW_BOOT:
                    oled_clear();
                    render_header();
                    render_centered_string(32, "EXPLORER");
                    oled_flush();
                    break;

                case UI_CMD_SHOW_MAIN_MENU:
                    oled_clear();
                    render_header();
                    oled_draw_string_5x7(10, 24, msg.data.main_menu.selected_index == 0 ? "> 1. APRENDER" : "  1. APRENDER");
                    oled_draw_string_5x7(10, 44, msg.data.main_menu.selected_index == 1 ? "> 2. TESTAR"   : "  2. TESTAR");
                    oled_flush();
                    break;

                case UI_CMD_SHOW_IR_LEARN:
                    oled_clear();
                    render_header();
                    oled_draw_string_5x7(15, 22, "APRENDER IR:");
                    if (msg.data.ir_step.action < UI_IR_CMD_MAX) {
                        render_centered_string(42, s_action_strings[msg.data.ir_step.action]);
                    }
                    oled_flush();
                    break;

                case UI_CMD_SHOW_IR_TEST:
                    oled_clear();
                    render_header();
                    oled_draw_string_5x7(20, 22, "TESTAR IR:");
                    if (msg.data.ir_step.action < UI_IR_CMD_MAX) {
                        render_centered_string(42, s_action_strings[msg.data.ir_step.action]);
                    }
                    oled_flush();
                    break;

                case UI_CMD_SHOW_WIFI_ERROR:
                    oled_clear();
                    render_header();
                    render_centered_string(24, "ERRO WIFI");
                    render_centered_string(44, "SEM CONEXAO!");
                    oled_flush();
                    break;

                case UI_CMD_SHOW_MESSAGE:
                    oled_clear();
                    render_header();
                    render_centered_string(24, msg.data.message.line1);
                    render_centered_string(44, msg.data.message.line2);
                    oled_flush();
                    if (msg.data.message.display_ms > 0) {
                        vTaskDelay(pdMS_TO_TICKS(msg.data.message.display_ms));
                    }
                    break;

                case UI_CMD_SHOW_SLEEP_PREP:
                    oled_clear();
                    render_header();
                    render_centered_string(32, "ENTRANDO SLEEP");
                    oled_flush();
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
 * SERVICE INIT & DEINIT
 * ========================================================================= */

esp_err_t app_ui_init(void) {
    ESP_LOGI(TAG, "Initializing Asynchronous UI Service...");
    
    esp_err_t ret = oled_init(OLED_I2C_ADDR_DEFAULT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize OLED driver: %s", esp_err_to_name(ret));
        return ret;
    }

    s_ui_queue = xQueueCreate(UI_QUEUE_LEN, sizeof(ui_msg_t));
    if (s_ui_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create UI queue!");
        return ESP_ERR_NO_MEM;
    }

    BaseType_t task_ret = xTaskCreate(app_ui_task, "app_ui_task", 3072, NULL, 4, &s_ui_task_handle);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create UI Task!");
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
 * POST API IMPLEMENTATIONS
 * ========================================================================= */

static esp_err_t send_to_ui_queue(const ui_msg_t *msg) {
    if (s_ui_queue == NULL) return ESP_ERR_INVALID_STATE;
    if (xQueueSend(s_ui_queue, msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "UI Queue Full! Dropping message.");
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

esp_err_t app_ui_post_ir_learn(ui_ir_cmd_action_t action) {
    ui_msg_t msg = { .type = UI_CMD_SHOW_IR_LEARN };
    msg.data.ir_step.action = action;
    return send_to_ui_queue(&msg);
}

esp_err_t app_ui_post_ir_test(ui_ir_cmd_action_t action) {
    ui_msg_t msg = { .type = UI_CMD_SHOW_IR_TEST };
    msg.data.ir_step.action = action;
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