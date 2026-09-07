idf_component_register(
    SRCS "bootloader_hooks.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES soc esp_rom
)