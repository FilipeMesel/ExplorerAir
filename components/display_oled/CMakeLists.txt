idf_component_register(
    SRCS "display_oled.c"
    INCLUDE_DIRS "include"
    REQUIRES board_i2c_bus driver esp_driver_i2c log
)