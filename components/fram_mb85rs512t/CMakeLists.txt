idf_component_register(
    SRCS "fram_mb85rs512t.c"
    INCLUDE_DIRS "include"
    REQUIRES board_spi_bus driver freertos
)