idf_component_register(
    SRCS "board_spi_bus.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_spi freertos
)