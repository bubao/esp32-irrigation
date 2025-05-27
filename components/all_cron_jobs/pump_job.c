#include "pump_job.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "pump_job";

pump_job_config_t* init_pump_job_task(pump_job_config_t* job_config)
{
    ESP_LOGI(TAG, "init: left=%d, right=%d, direction=%d, duration=%d",
        job_config->left_gpio_num,
        job_config->right_gpio_num,
        job_config->direction,
        job_config->duration_ms);

    gpio_set_direction(job_config->left_gpio_num, GPIO_MODE_OUTPUT);
    gpio_set_direction(job_config->right_gpio_num, GPIO_MODE_OUTPUT);
    gpio_set_level(job_config->left_gpio_num, 0);
    gpio_set_level(job_config->right_gpio_num, 0);

    return job_config;
}

void job_task_cb(cron_job* job)
{
    pump_job_config_t* data = (pump_job_config_t*)job->data;

    if (!data) {
        ESP_LOGE(TAG, "No data in job->data!");
        return;
    }

    ESP_LOGI(TAG, "run left=%d, right=%d, direction=%d, duration=%d",
        data->left_gpio_num,
        data->right_gpio_num,
        data->direction,
        data->duration_ms);

    gpio_num_t pin = (data->direction == 0) ? data->left_gpio_num : data->right_gpio_num;
    gpio_set_level(pin, 1);
    vTaskDelay(pdMS_TO_TICKS(data->duration_ms));
    gpio_set_level(pin, 0);

    ESP_LOGI(TAG, "end");
}
