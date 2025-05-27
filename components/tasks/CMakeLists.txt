idf_component_register(
    SRCS "pump_tasks.c"
    INCLUDE_DIRS "."
    REQUIRES esp_schedule driver
)