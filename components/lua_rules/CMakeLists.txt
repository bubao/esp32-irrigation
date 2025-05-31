idf_component_register(
    SRCS "lua_functions.c"  "lua_bindings.c" 
    INCLUDE_DIRS "."
	REQUIRES lua driver esp_adc esp_event esp_http_client spi_flash fatfs littlefs esp_driver_spi esp_timer
)
