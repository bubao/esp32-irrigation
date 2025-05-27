idf_component_register(
    SRCS "pump_job.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_cron
)
