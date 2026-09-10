#include "rtc_ht8563.h"
#include "board_i2c_bus.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include <string.h>

static const char *TAG = "RTC_HT8563";

#define REG_CTRL1      0x00
#define REG_CTRL2      0x01

#define REG_SEC        0x02
#define REG_MIN        0x03
#define REG_HOUR       0x04
#define REG_DAY        0x05
#define REG_WEEK       0x06
#define REG_MONTH      0x07
#define REG_YEAR       0x08

#define REG_MIN_ALARM  0x09
#define REG_HOUR_ALARM 0x0A
#define REG_DAY_ALARM  0x0B
#define REG_WEEK_ALARM 0x0C

#define REG_TIMER_CTRL 0x0E
#define REG_TIMER_VAL  0x0F

#define TIMER_CLK_4096HZ   0x00
#define TIMER_CLK_64HZ     0x01
#define TIMER_CLK_1HZ      0x02
#define TIMER_CLK_1PER60HZ 0x03  // 1/60 Hz (1 pulso por minuto)
#define TIMER_ENABLE_BIT   0x80  // TE Bit (Bit 7)

static i2c_master_dev_handle_t s_rtc_dev_handle = NULL;

uint8_t rtc_dec_to_bcd(uint8_t val) {
    return ((val / 10) << 4) | (val % 10);
}

uint8_t rtc_bcd_to_dec(uint8_t val) {
    return ((val >> 4) * 10) + (val & 0x0F);
}

esp_err_t rtc_ht8563_init(void) {
    if (s_rtc_dev_handle != NULL) {
        return ESP_OK;
    }

    i2c_master_bus_handle_t bus_handle = board_i2c_bus_get_handle();
    if (!bus_handle) {
        ESP_LOGE(TAG, "Barramento I2C não inicializado");
        return ESP_ERR_INVALID_STATE;
    }

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = RTC_I2C_ADDR,
        .scl_speed_hz = 100000,
    };

    esp_err_t ret = i2c_master_bus_add_device(bus_handle, &dev_config, &s_rtc_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao adicionar dispositivo RTC no barramento");
        return ret;
    }

    ESP_LOGI(TAG, "RTC HT8563 inicializado com sucesso (0x%02X)", RTC_I2C_ADDR);
    // rtc_ht8563_write_reg(REG_CTRL2, 0x00);
    return ESP_OK;
}

esp_err_t rtc_ht8563_deinit(void) {
    if (s_rtc_dev_handle != NULL) {
        esp_err_t ret = i2c_master_bus_rm_device(s_rtc_dev_handle);
        if (ret == ESP_OK) {
            s_rtc_dev_handle = NULL;
            ESP_LOGI(TAG, "Dispositivo RTC removido do barramento I2C.");
        }
        return ret;
    }
    return ESP_OK;
}

esp_err_t rtc_ht8563_read_reg(uint8_t reg, uint8_t *data) {
    if (!s_rtc_dev_handle) return ESP_ERR_INVALID_STATE;
    return i2c_master_transmit_receive(s_rtc_dev_handle, &reg, 1, data, 1, -1);
}

esp_err_t rtc_ht8563_write_reg(uint8_t reg, uint8_t data) {
    if (!s_rtc_dev_handle) return ESP_ERR_INVALID_STATE;
    uint8_t write_buf[2] = {reg, data};
    return i2c_master_transmit(s_rtc_dev_handle, write_buf, sizeof(write_buf), -1);
}

esp_err_t rtc_ht8563_clear_flags(void) {
    uint8_t ctrl2 = 0;
    esp_err_t ret = rtc_ht8563_read_reg(REG_CTRL2, &ctrl2);
    if (ret == ESP_OK) {
        ctrl2 &= ~(1 << 3); // Bit 3 = AF (Alarm Flag)
        ctrl2 &= ~(1 << 2); // Bit 2 = TF (Timer Flag)
        ctrl2 |= (1 << 1);  // Bit 1 = AIE (Alarm Interrupt Enable)
        ret = rtc_ht8563_write_reg(REG_CTRL2, ctrl2);
        ESP_LOGI(TAG, "Flags AF e TF limpas e interrupção habilitada.");
    }
    return ret;
}

esp_err_t rtc_ht8563_set_time(const rtc_date_time_t *dt) {
    esp_err_t ret;
    ret = rtc_ht8563_write_reg(REG_SEC, rtc_dec_to_bcd(dt->second) & 0x7F);
    ret |= rtc_ht8563_write_reg(REG_MIN, rtc_dec_to_bcd(dt->minute) & 0x7F);
    ret |= rtc_ht8563_write_reg(REG_HOUR, rtc_dec_to_bcd(dt->hour) & 0x3F);
    ret |= rtc_ht8563_write_reg(REG_DAY, rtc_dec_to_bcd(dt->day) & 0x3F);
    ret |= rtc_ht8563_write_reg(REG_WEEK, rtc_dec_to_bcd(dt->weekday) & 0x07);
    ret |= rtc_ht8563_write_reg(REG_MONTH, rtc_dec_to_bcd(dt->month) & 0x1F);
    ret |= rtc_ht8563_write_reg(REG_YEAR, rtc_dec_to_bcd(dt->year % 100));
    return ret;
}

esp_err_t rtc_ht8563_get_time(rtc_date_time_t *dt) {
    uint8_t raw[7];
    uint8_t start_reg = REG_SEC;
    esp_err_t ret = i2c_master_transmit_receive(s_rtc_dev_handle, &start_reg, 1, raw, 7, -1);
    if (ret == ESP_OK) {
        dt->second  = rtc_bcd_to_dec(raw[0] & 0x7F);
        dt->minute  = rtc_bcd_to_dec(raw[1] & 0x7F);
        dt->hour    = rtc_bcd_to_dec(raw[2] & 0x3F);
        dt->day     = rtc_bcd_to_dec(raw[3] & 0x3F);
        dt->weekday = rtc_bcd_to_dec(raw[4] & 0x07);
        dt->month   = rtc_bcd_to_dec(raw[5] & 0x1F);
        dt->year    = 2000 + rtc_bcd_to_dec(raw[6]);
    }
    return ret;
}

esp_err_t rtc_ht8563_set_alarm(uint8_t hour, uint8_t minute) {
    uint8_t min_bcd = rtc_dec_to_bcd(minute) & 0x7F;
    uint8_t hour_bcd = rtc_dec_to_bcd(hour) & 0x3F;

    rtc_ht8563_write_reg(REG_MIN_ALARM, min_bcd);
    rtc_ht8563_write_reg(REG_HOUR_ALARM, hour_bcd);
    rtc_ht8563_write_reg(REG_DAY_ALARM, 0x80);
    rtc_ht8563_write_reg(REG_WEEK_ALARM, 0x80);

    ESP_LOGI(TAG, "Alarme programado para as %02d:%02d:00", hour, minute);
    return ESP_OK;
}

// esp_err_t rtc_ht8563_set_timer(uint8_t seconds) {
//     uint8_t ctrl2 = 0;

//     // 1. Limpa o registrador Control/Status 2 (Desabilita interrupção, limpa flags TF/AF)
//     rtc_ht8563_write_reg(REG_CTRL2, 0x00);

//     // 1. Disable the timer for configuration
//     rtc_ht8563_write_reg(REG_TIMER_CTRL, 0x00);

//     // 2. Load the value for the countdown timer
//     rtc_ht8563_write_reg(REG_TIMER_VAL, seconds);

//     // 3. Enable the Timer (Bit 7 TE = 1) and configure the time base to 1 Hz (Bits 1:0 = 10)
//     // Bit 3 (TI/TP = 0): Generates a maintained level signal on the /INT pin until cleared via software
//     rtc_ht8563_write_reg(REG_TIMER_CTRL, 0x82);

//     // 4. Habilita a interrupção do Timer no pino INT em MODO PULSO (TI_TP = 1, TIE = 1 -> 0x11)
//     // Bit 4 (TI_TP = 1) gera pulso temporizado no pino INT
//     // Bit 0 (TIE = 1) habilita interrupção do Timer
//     rtc_ht8563_write_reg(REG_CTRL2, 0x11);

//     // 5. Habilita o Timer com frequência base de 1 Hz (TE = 1 [bit 7], TD = 10 [bits 1:0])
//     // 0x82 -> 1000 0010 (TE=1, TD1=1, TD0=0 -> Clock de 1 Hz)
//     rtc_ht8563_write_reg(REG_TIMER_CTRL, 0x82);

//     ESP_LOGI(TAG, "Timer Countdown de %ds configurado no RTC.", seconds);
//     return ESP_OK;
// }

esp_err_t rtc_ht8563_set_timer(uint32_t seconds) {
    if (seconds == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t timer_ctrl = 0;
    uint8_t countdown_value = 0;

    // Lógica para chaveamento automático de escala
    if (seconds <= 255) {
        // Usa a base de tempo de 1 Hz (Segundos)
        timer_ctrl = TIMER_CLK_1HZ | TIMER_ENABLE_BIT; // 0x82
        countdown_value = (uint8_t)seconds;
    } 
    else if (seconds <= (255 * 60)) { // Até 15.300s (255 minutos ou 4.25 horas)
        // Usa a base de tempo de 1/60 Hz (Minutos)
        timer_ctrl = TIMER_CLK_1PER60HZ | TIMER_ENABLE_BIT; // 0x83
        
        // Converte segundos para minutos (arredondando para cima para garantir cobertura mínima)
        uint32_t minutes = (seconds + 59) / 60; 
        if (minutes > 255) minutes = 255;
        
        countdown_value = (uint8_t)minutes;
    } 
    else {
        ESP_LOGE("RTC_HT8563", "Intervalo solicitado (%lu s) excede o limite máximo do timer (15300s).", seconds);
        return ESP_ERR_INVALID_ARG;
    }

    board_i2c_bus_lock(100);

    // 1. Limpa o REG_CTRL2 (limpa flags antigas TF/AF e desabilita interrupções anteriores)
    uint8_t buf_ctrl2_clear[2] = {REG_CTRL2, 0x00};
    esp_err_t ret = i2c_master_transmit(s_rtc_dev_handle, buf_ctrl2_clear, sizeof(buf_ctrl2_clear), 100);
    if (ret != ESP_OK) { board_i2c_bus_unlock(); return ret; }

    // 2. Desabilita temporariamente o Timer para aplicar a nova contagem
    uint8_t buf_timer_disable[2] = {REG_TIMER_CTRL, 0x00};
    ret = i2c_master_transmit(s_rtc_dev_handle, buf_timer_disable, sizeof(buf_timer_disable), 100);
    if (ret != ESP_OK) { board_i2c_bus_unlock(); return ret; }

    // 3. Escreve o valor inicial da contagem regressiva
    uint8_t buf_count[2] = {REG_TIMER_VAL, countdown_value};
    ret = i2c_master_transmit(s_rtc_dev_handle, buf_count, sizeof(buf_count), 100);
    if (ret != ESP_OK) { board_i2c_bus_unlock(); return ret; }

    // 4. Habilita a interrupção do Timer no pino INT em MODO PULSO (TI_TP = 1, TIE = 1 -> 0x11)
    uint8_t buf_ctrl2_enable[2] = {REG_CTRL2, 0x11};
    ret = i2c_master_transmit(s_rtc_dev_handle, buf_ctrl2_enable, sizeof(buf_ctrl2_enable), 100);
    if (ret != ESP_OK) { board_i2c_bus_unlock(); return ret; }

    // 5. Liga o Timer ativando o bit TE e definindo a base de frequência (1Hz ou 1/60Hz)
    uint8_t buf_ctrl[2] = {REG_TIMER_CTRL, timer_ctrl};
    ret = i2c_master_transmit(s_rtc_dev_handle, buf_ctrl, sizeof(buf_ctrl), 100);

    board_i2c_bus_unlock();

    ESP_LOGI("RTC_HT8563", "Timer do RTC setado para %lu segundos (Valor reg: %d, Modo: %s)", 
             seconds, countdown_value, (timer_ctrl & 0x03) == TIMER_CLK_1HZ ? "Segundos" : "Minutos");

    return ret;
}

/* Tests */
void rtc_ht8563_run_test_log_datetime(void) {
    rtc_date_time_t dt;
    if (rtc_ht8563_get_time(&dt) == ESP_OK) {
        ESP_LOGI(TAG, "[TESTE 1] Data/Hora Atual: %02d/%02d/%04d %02d:%02d:%02d",
                 dt.day, dt.month, dt.year, dt.hour, dt.minute, dt.second);
    } else {
        ESP_LOGE(TAG, "[TESTE 1] Falha ao ler data/hora do RTC");
    }
}

void rtc_ht8563_run_test_alarm(uint8_t hour, uint8_t minute) {
    ESP_LOGI(TAG, "[TESTE 2] Limpando flags e configurando alarme...");
    rtc_ht8563_clear_flags();
    rtc_ht8563_set_alarm(hour, minute);
}

void rtc_ht8563_run_test_timer(uint8_t seconds) {
    ESP_LOGI(TAG, "[TESTE 3] Limpando flags e configurando timer de %ds...", seconds);
    rtc_ht8563_clear_flags();
    rtc_ht8563_set_timer(seconds);
}

esp_err_t rtc_ht8563_get_flags(bool *timer_flag, bool *alarm_flag) {
    if (!timer_flag || !alarm_flag) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t ctrl2 = 0;
    esp_err_t ret = rtc_ht8563_read_reg(REG_CTRL2, &ctrl2);
    if (ret == ESP_OK) {
        *alarm_flag = (ctrl2 & (1 << 3)) != 0; // Bit 3 = AF
        *timer_flag = (ctrl2 & (1 << 2)) != 0; // Bit 2 = TF
    }
    return ret;
}

void rtc_ht8563_run_configured_tests(void) {
    ESP_LOGI(TAG, "=== Iniciando Bateria de Testes do RTC HT8563 ===");

#ifdef CONFIG_RTC_HT8563_TEST_READ_DATETIME
    ESP_LOGI(TAG, "[Kconfig Test 1] Executando Teste de Leitura de Data/Hora...");
    rtc_date_time_t dt;
    if (rtc_ht8563_get_time(&dt) == ESP_OK) {
        ESP_LOGI(TAG, "RTC Horário Atual: %02d/%02d/%04d %02d:%02d:%02d",
                 dt.day, dt.month, dt.year, dt.hour, dt.minute, dt.second);
    } else {
        ESP_LOGE(TAG, "Falha na leitura do RTC!");
    }
#endif

#ifdef CONFIG_RTC_HT8563_TEST_ALARM_INT
    ESP_LOGI(TAG, "[Kconfig Test 2] Executando Teste de Alarme de Hora...");
    rtc_ht8563_clear_flags();
    
    rtc_date_time_t dt_alarm;
    if (rtc_ht8563_get_time(&dt_alarm) == ESP_OK) {
        uint8_t alarm_min = (dt_alarm.minute + 1) % 60;
        uint8_t alarm_hour = (alarm_min == 0) ? (dt_alarm.hour + 1) % 24 : dt_alarm.hour;
        
        rtc_ht8563_set_alarm(alarm_hour, alarm_min);
        ESP_LOGI(TAG, "Alarme de teste agendado para %02d:%02d:00", alarm_hour, alarm_min);
    }
#endif

#ifdef CONFIG_RTC_HT8563_TEST_TIMER_INT
    ESP_LOGI(TAG, "[Kconfig Test 3] Executando Teste de Timer Countdown (10s)...");
    rtc_ht8563_clear_flags();
    rtc_ht8563_set_timer(10);
#endif

    ESP_LOGI(TAG, "=== Bateria de Testes Concluída ===");
}