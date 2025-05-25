idf_component_register(
    SRCS "esp32_info.c"
    INCLUDE_DIRS "."
    REQUIRES spi_flash esp_timer time_sync
)