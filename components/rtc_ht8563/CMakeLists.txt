idf_component_register(
    SRCS "rtc_ht8563.c"
    INCLUDE_DIRS "include"
    REQUIRES board_i2c_bus esp_driver_i2c
)