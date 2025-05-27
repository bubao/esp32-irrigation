#include "pump_job.h"
#include "cron.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "soc/gpio_num.h"

pump_job_config_t init_pump_job_task(pump_job_config_t* job_config)
{
    gpio_set_direction(job_config->left_gpio_num | job_config->right_gpio_num, GPIO_MODE_OUTPUT);
    gpio_set_level(job_config->left_gpio_num | job_config->right_gpio_num, 0);

    return (pump_job_config_t) {
        .left_gpio_num = job_config->left_gpio_num,
        .right_gpio_num = job_config->right_gpio_num,
        .direction = job_config->direction,
        .duration_ms = job_config->duration_ms,
    };
}

void job_task_cb(pump_job_config_t* job)
{
    gpio_set_level(job->direction == 1 ? job->left_gpio_num : job->right_gpio_num, 1);
    vTaskDelay(pdMS_TO_TICKS(job->duration_ms));
    gpio_set_level(job->direction == 1 ? job->left_gpio_num : job->right_gpio_num, 0);
}