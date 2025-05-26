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

        xTaskCreate(print_info_task, "info_task", 2048, NULL, 5, NULL);
        xTaskCreate(time_sync_task, "sync_time_task", 4096, NULL, 5, NULL);
    } else {
        ESP_LOGW(TAG, "Unknown WiFi mode.");
    }
}
