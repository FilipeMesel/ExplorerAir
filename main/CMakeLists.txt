idf_component_register(SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES board_spi_bus board_i2c_bus fram_mb85rs512t display_oled rtc_ht8563 nvs_flash board_wifi board_mqtt)
