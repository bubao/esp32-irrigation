#ifndef PUMP_JOB_H
#define PUMP_JOB_H

#include "driver/gpio.h"
#include "cron.h"

typedef struct {
    gpio_num_t left_gpio_num;
    gpio_num_t right_gpio_num;
	int direction; // 0 left or 1 right
	int duration_ms; // 持续时间，单位毫秒
} pump_job_config_t;

pump_job_config_t* init_pump_job_task(pump_job_config_t* job_config);

void job_task_cb(cron_job* job_config);

// void cron_job_adapter(cron_job* job_config);
#endif // PUMP_JOB_H