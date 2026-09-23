idf_component_register(
    SRCS "ir_remote.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_driver_rmt esp_driver_gpio freertos
)