#ifndef PUMP_TASKS_H
#define PUMP_TASKS_H

#include "driver/gpio.h"

void register_pump_schedule_task(gpio_num_t gpio, const char* name, int duration_ms, int hour, int min);

#endif // PUMP_TASKS_H
