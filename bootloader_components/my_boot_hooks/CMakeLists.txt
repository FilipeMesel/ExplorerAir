idf_component_register(
    SRCS "hooks.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES soc esp_rom hal
)

# FORÇA o sistema a embutir o binário inteiro do hook, sem descartar funções soltas
idf_component_set_property(${COMPONENT_NAME} WHOLE_ARCHIVE 1)
