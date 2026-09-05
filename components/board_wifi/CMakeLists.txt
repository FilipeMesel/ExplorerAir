idf_component_register(
    SRCS "board_wifi.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_event esp_wifi esp_netif
)