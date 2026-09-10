idf_component_register(
    SRCS "board_i2c_bus.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2c freertos
)