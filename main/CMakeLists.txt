idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp-wifi-wrapper esp_event time_sync esp32_info driver all_cron_jobs esp_cron)