#include "cron.h"
#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/gpio_types.h"
#include "lua_functions.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "pump_job.h"
#include "soc/gpio_num.h"
#include "time_sync.h"
#include "wifi_connect.h"

static const char* TAG = "main";

static gpio_num_t led_gpio = GPIO_NUM_15;

static void time_tasks(void* pvParameters)
{
    static pump_job_config_t job_config = {
        .left_gpio_num = GPIO_NUM_4,
        .right_gpio_num = GPIO_NUM_5,
        .direction = 1,
        .duration_ms = 10000,
    };

    init_pump_job_task(&job_config);

    // 等待时间同步
    while (!is_time_synced()) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    cron_job_create("0 0 8,18 * * *", job_task_cb, &job_config);
    gpio_set_direction(led_gpio, GPIO_MODE_INPUT_OUTPUT);

    // 每分钟执行一次任务
    // cron_job_create("*/20 * * * * *", job_task_cb, &job_config);
    cron_start();

    while (1) {
        // 你可以在这里添加其他任务逻辑
        gpio_set_level(led_gpio, 1);
        vTaskDelay(pdMS_TO_TICKS(1000));
        gpio_set_level(led_gpio, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void lua_tasks(void* pvParameters)
{
    // 初始化SPIFFS文件系统
    init_spiffs();

    // 初始化Lua环境
    lua_State* L = init_lua();

    // 自动注册Lua规则文件
    register_all_lua_scripts(L, "/assets");

    // 启动Lua任务
    start_lua_task(L);
    while (1) {
        // 这里可以添加其他Lua任务逻辑
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    lua_close(L);
}

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
        xTaskCreate(lua_tasks, "lua_tasks", 4096, NULL, 5, NULL);
    } else {
        ESP_LOGW(TAG, "未知 WiFi 连接模式");
    }
}
