#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_schedule.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <string.h>

static const char* TAG = "pump_task";

typedef struct {
    gpio_num_t gpio;
    int duration_ms;
    char name[32];
} pump_task_priv_t;

static void pump_control_task(void* arg)
{
    pump_task_priv_t* data = (pump_task_priv_t*)arg;

    // 打印当前任务剩余堆栈空间（单位：字节）
    UBaseType_t stack_left = uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t);
    ESP_LOGI(TAG, "Pump task started: %s, stack left: %u bytes", data->name, stack_left);

    gpio_set_level(data->gpio, 1);
    ESP_LOGI(TAG, "Pump %s ON", data->name);

    vTaskDelay(pdMS_TO_TICKS(data->duration_ms));

    gpio_set_level(data->gpio, 0);
    ESP_LOGI(TAG, "Pump %s OFF", data->name);

    // free(data);
    vTaskDelete(NULL);
}

static void pump_schedule_cb(esp_schedule_handle_t handle, void* priv_data)
{
    pump_task_priv_t* original = (pump_task_priv_t*)priv_data;

    pump_task_priv_t* task_data = malloc(sizeof(pump_task_priv_t));
    if (!task_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for pump task");
        return;
    }

    *task_data = *original; // 复制结构体，避免并发冲突
    ESP_LOGI(TAG, "Schedule triggered for %s", task_data->name);

    if (xTaskCreate(pump_control_task, "pump_task", 2048, task_data, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create pump task for %s", task_data->name);
        // free(task_data);
    } else {
        ESP_LOGI(TAG, "Pump task created for %s", task_data->name);
    }
}

void register_pump_schedule_task(gpio_num_t gpio, const char* name, int duration_ms, int hour, int min)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << gpio,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(gpio, 0);

    pump_task_priv_t* priv_data = malloc(sizeof(pump_task_priv_t));
    if (!priv_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for pump schedule");
        return;
    }

    priv_data->gpio = gpio;
    priv_data->duration_ms = duration_ms;
    strncpy(priv_data->name, name, sizeof(priv_data->name) - 1);
    priv_data->name[sizeof(priv_data->name) - 1] = '\0';

    esp_schedule_config_t sch_cfg = {
        .name = "",
        .trigger = {
            .type = ESP_SCHEDULE_TYPE_DAYS_OF_WEEK,
            .hours = hour,
            .minutes = min,
            .day.repeat_days = ESP_SCHEDULE_DAY_EVERYDAY,
        },
        .trigger_cb = pump_schedule_cb,
        .priv_data = priv_data,
    };
    strncpy(sch_cfg.name, name, sizeof(sch_cfg.name) - 1);
    sch_cfg.name[sizeof(sch_cfg.name) - 1] = '\0';

    esp_schedule_handle_t handle = esp_schedule_create(&sch_cfg);
    if (!handle) {
        ESP_LOGE(TAG, "Failed to create schedule for %s", name);
        // free(priv_data);
    } else {
        // esp_err_t err = esp_schedule_enable(handle);
        // if (err != ESP_OK) {
        //     ESP_LOGE(TAG, "Failed to enable schedule for %s, err=0x%x", name, err);
        // } else {
        //     ESP_LOGI(TAG, "Schedule created and enabled for %s at %02d:%02d", name, hour, min);
        // }
    }
}

/**
 * FreeRTOS 栈溢出检测钩子函数
 * 需要在 FreeRTOSConfig.h 中启用：
 * #define configCHECK_FOR_STACK_OVERFLOW 2
 */
// void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
// {
//     ESP_LOGE(TAG, "Stack overflow detected in task: %s", pcTaskName);
//     abort(); // 或者调用 esp_restart() 重启
// }
