idf_component_register(
    SRCS "board_mqtt.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_event mqtt
)