idf_component_register(
    SRCS 
        "explorer_irblaster.c"
        "display.c"
        "ir_handler.c"
        "wifi_handler.c"
        "mqtt_handler.c"
        "explorer_memory.c"
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

if(ENABLE_FRAM_STORAGE)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE _INCLUDE_FRAM_)
    message(STATUS "[main] Persistência: FRAM (_INCLUDE_FRAM_)")
else()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE _INCLUDE_NVS_)
    message(STATUS "[main] Persistência: NVS (_INCLUDE_NVS_)")
endif()