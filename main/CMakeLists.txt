idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp-wifi-wrapper esp_event time_sync esp32_info driver all_cron_jobs esp_cron lua_rules)

spiffs_create_partition_image(assets "../assets" LITTLEFS)
