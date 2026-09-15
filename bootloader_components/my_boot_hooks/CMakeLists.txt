idf_component_register(
    SRCS "hooks.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES soc esp_rom hal
)

# Forces the system to embed the entire hook binary, without discarding unused functions.
idf_component_set_property(${COMPONENT_NAME} WHOLE_ARCHIVE 1)
