#include "cron.h"
#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "pump_job.h"
#include "time_sync.h"
#include "wifi_connect.h"
#include <stdio.h>

static const char* TAG = "main";

static void time_tasks(void* pvParameters)
{
    // esp_rom_gpio_pad_select_gpio(GPIO_NUM_15);
    pump_job_config_t job_config = {
        .left_gpio_num = 3,
        .right_gpio_num = 4,
        .direction = 0,
        .duration_ms = 10000,
    };
    init_pump_job_task(&job_config);

    while (is_time_synced() == false) {
        vTaskDelay(pdMS_TO_TICKS(1000)); // 每秒检查一次
    }
    cron_job_create("0 0 8,18 * * *", (cron_job_callback)job_task_cb, &job_config);
    cron_start();

    while (1) {
        // 这里可以添加其他任务逻辑
        vTaskDelay(pdMS_TO_TICKS(1000)); // 每秒执行一次
    }
}

void app_main(void)
{
    // 初始化事件循环
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 初始化 NVS（WiFi配置需要）
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 连接 WiFi
    wifi_connect_mode_t mode = wifi_connect();

    if (mode == WIFI_CONNECT_MODE_AP) {
        ESP_LOGI(TAG, "Entered WiFi AP configuration mode.");
    } else if (mode == WIFI_CONNECT_MODE_STA) {
        ESP_LOGI(TAG, "WiFi connected, starting time sync and info task.");

        xTaskCreate(time_sync_task, "sync_time_task", 4096, NULL, 5, NULL);
        xTaskCreate(time_tasks, "task", 4096, NULL, 5, NULL);
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    } else {
        ESP_LOGW(TAG, "Unknown WiFi mode.");
    }
}
