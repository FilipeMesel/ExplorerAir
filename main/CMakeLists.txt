idf_component_register(
    SRCS 
        "explorer_irblaster.c"
        "display.c"
        "ir_handler.c"
        "mqtt_handler.c"
    INCLUDE_DIRS 
        "."
    REQUIRES 
        espressif__cjson
        esp_driver_gpio # Requisito adicionado para resolver o erro
        esp_driver_rmt  # Driver RMT
        esp_driver_i2c  # Driver I2C
        nvs_flash       # Memória NVS
        esp_wifi        # Wi-Fi
        esp_event       # Loop de Eventos
        espressif__mqtt # Componente MQTT
)