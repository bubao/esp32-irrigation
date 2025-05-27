#include "cron.h"
#include "driver/gpio.h"
#include "esp32_info.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "time_sync.h"
#include "wifi_connect.h"
#include <inttypes.h>
#include <stdio.h>

static const char* TAG = "main";
static esp32_info_t esp32_info; // 非 const，全局结构体变量

static void print_info_task(void* pvParameters)
{
    while (1) {
        print_esp32_info(&esp32_info);
        vTaskDelay(pdMS_TO_TICKS(5000)); // 每5秒打印一次
    }
}

void example_cb(cron_job* job)
{
    // const char* msg = (const char*)job->data; // 使用 job->arg 获取传递的参数
    // ESP_LOGI(TAG, "Task fired! message: %s", msg);
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    // 读取当前状态并取反
    int level = gpio_get_level(GPIO_NUM_15);
    ESP_LOGI("TAG", "gpio_get_level: %d Current time: %s", level, asctime(&timeinfo));

    gpio_set_level(GPIO_NUM_15, level == 1 ? 0 : 1);
}

static void time_tasks(void* pvParameters)
{
    // esp_rom_gpio_pad_select_gpio(GPIO_NUM_15);
    gpio_set_direction(GPIO_NUM_15, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_level(GPIO_NUM_15, 0);
    while (is_time_synced() == false) {
        // ESP_LOGI(TAG, "Waiting for time sync...");
        vTaskDelay(pdMS_TO_TICKS(1000)); // 每秒检查一次
    }
    cron_job_create("10,30,50 * * * * *", example_cb, "Hello from cron!");
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
        get_esp32_info(&esp32_info);

        // xTaskCreate(print_info_task, "info_task", 2048, NULL, 5, NULL);
        xTaskCreate(time_sync_task, "sync_time_task", 4096, NULL, 5, NULL);
        xTaskCreate(time_tasks, "task", 4096, NULL, 5, NULL);
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    } else {
        ESP_LOGW(TAG, "Unknown WiFi mode.");
    }
}
