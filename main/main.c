#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lua_functions.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "time_sync.h"
#include "wifi_connect.h"

static const char* TAG = "main";

void app_main(void)
{
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 初始化 NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 连接 Wi-Fi
    wifi_connect_mode_t mode = wifi_connect();

    if (mode == WIFI_CONNECT_MODE_AP) {
        ESP_LOGI(TAG, "WiFi AP模式，等待配置");
    } else if (mode == WIFI_CONNECT_MODE_STA) {
        ESP_LOGI(TAG, "WiFi连接成功，启动时间同步与任务调度");

        xTaskCreate(time_sync_task, "sync_time_task", 4096, NULL, 5, NULL);
        init_littlefs();

        start_lua_system(); // 启动Lua任务

    } else {
        ESP_LOGW(TAG, "未知 WiFi 连接模式");
    }
    while (1) {
        // 这里可以添加其他Lua任务逻辑
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
