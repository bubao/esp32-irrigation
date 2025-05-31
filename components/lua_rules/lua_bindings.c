#include "lua_bindings.h"
#include "dirent.h"
#include "driver/adc.h"
#include "driver/gpio.h"
#include "driver/mcpwm.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include "esp_spi_flash.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lauxlib.h"
#include "lua.h"
#include "lua_bindings.h"
#include "lua_functions.h"
#include "rom/gpio.h"
#include "soc/gpio_num.h"
#include <dirent.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

// Lua: settimeout(ms)

typedef struct timer_task {
    lua_State* co;
    int wakeup_time_ms;
    struct timer_task* next;
} timer_task_t;

static timer_task_t* timer_list = NULL;

// settimeout(ms)，挂起当前协程，ms后恢复，按时间升序插入链表
static int l_settimeout(lua_State* L)
{
    int ms = luaL_checkinteger(L, 1);
    // 获取当前协程状态
    lua_State* co = L;

    // 找到对应协程记录，设置唤醒时间
    int64_t now = esp_timer_get_time(); // 微秒
    for (int i = 0; i < coroutine_count; i++) {
        if (coroutines[i].co == co) {
            coroutines[i].wake_up_time_us = now + ms * 1000;
            break;
        }
    }

    // 挂起协程
    return lua_yield(L, 0);
}

// 定时器轮询，唤醒过期协程（调用此函数需要在主循环或定时器任务里）
void timer_process(lua_State* L)
{
    int64_t now_us = esp_timer_get_time();
    int now_ms = (int)(now_us / 1000);

    timer_task_t* prev = NULL;
    timer_task_t* curr = timer_list;

    while (curr) {
        if (curr->wakeup_time_ms <= now_ms) {
            // 从链表移除curr节点
            if (prev) {
                prev->next = curr->next;
            } else {
                timer_list = curr->next;
            }

            int nresults = 0;
            int ret = lua_resume(curr->co, NULL, 0, &nresults);

            if (ret == LUA_YIELD) {
                // 协程还在挂起状态，重新加入链表尾部
                timer_task_t* next = curr->next;
                curr->next = NULL;

                // 重新插入timer_list尾部
                if (!timer_list) {
                    timer_list = curr;
                } else {
                    timer_task_t* p = timer_list;
                    while (p->next)
                        p = p->next;
                    p->next = curr;
                }

                curr = next;
                // prev不变
            } else {
                // 协程结束或出错，释放task
                if (ret != LUA_OK) {
                    const char* err = lua_tostring(curr->co, -1);
                    ESP_LOGE("TIMER", "Coroutine resume error: %s", err);
                    lua_pop(curr->co, 1);
                }
                timer_task_t* to_free = curr;
                curr = curr->next;
                free(to_free);
                // prev不变
            }
        } else {
            prev = curr;
            curr = curr->next;
        }
    }
}

// Lua函数：设置GPIO模式
static int l_gpio_set_mode(lua_State* L)
{
    gpio_num_t gpio_num = (gpio_num_t)luaL_checkinteger(L, 1);
    gpio_mode_t mode = (gpio_mode_t)luaL_checkinteger(L, 2);
    gpio_pad_select_gpio(gpio_num);
    gpio_set_direction(gpio_num, (gpio_mode_t)mode);
    return 0;
}

// Lua函数：设置GPIO电平
static int l_gpio_set_level(lua_State* L)
{
    int gpio_num = luaL_checkinteger(L, 1);
    int level = luaL_checkinteger(L, 2);
    gpio_set_level(gpio_num, level);
    return 0;
}

// Lua函数：读取GPIO电平
static int l_gpio_get_level(lua_State* L)
{
    int gpio_num = luaL_checkinteger(L, 1);
    int level = gpio_get_level(gpio_num);
    lua_pushinteger(L, level);
    return 1;
}

// Lua函数：设置PWM
// static int l_pwm_set_duty(lua_State* L)
// {
//     int pwm_channel = luaL_checkinteger(L, 1);
//     int duty = luaL_checkinteger(L, 2);
//     mcpwm_set_duty(MCPWM_UNIT_0, (mcpwm_timer_t)pwm_channel, MCPWM_OPR_A, duty);
//     return 0;
// }

// Lua函数：读取ADC信号
static int l_read_adc(lua_State* L)
{
    int adc_channel = luaL_checkinteger(L, 1);
    adc1_channel_t channel = (adc1_channel_t)adc_channel;
    int adc_value = adc1_get_raw(channel);
    lua_pushinteger(L, adc_value);
    return 1;
}

// Lua函数：读取文件内容
static int l_file_read(lua_State* L)
{
    const char* filename = luaL_checkstring(L, 1);
    FILE* file = fopen(filename, "r");
    if (!file) {
        lua_pushnil(L);
        return 1;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* buffer = (char*)malloc(size + 1);
    fread(buffer, 1, size, file);
    fclose(file);
    buffer[size] = '\0';
    lua_pushstring(L, buffer);
    free(buffer);
    return 1;
}

// Lua函数：写入文件内容
static int l_file_write(lua_State* L)
{
    const char* filename = luaL_checkstring(L, 1);
    const char* content = luaL_checkstring(L, 2);
    FILE* file = fopen(filename, "w");
    if (!file) {
        lua_pushboolean(L, 0);
        return 1;
    }
    fwrite(content, 1, strlen(content), file);
    fclose(file);
    lua_pushboolean(L, 1);
    return 1;
}

// 模拟传感器读取
static int l_read_sensor(lua_State* L)
{
    static int value = 0;
    value = (value + 10) % 100;
    lua_pushinteger(L, value);
    return 1; // ✅ 表示有1个返回值传给 Lua
}

// 注册绑定函数
void register_lua_bindings(lua_State* L)
{

    lua_newtable(L);
    // 注册gpio.set_mode函数
    lua_pushcfunction(L, l_gpio_set_mode);
    lua_setfield(L, -2, "set_mode");

    // 注册gpio.set_level函数
    lua_pushcfunction(L, l_gpio_set_level);
    lua_setfield(L, -2, "set_level");

    // 注册gpio.get_level函数
    lua_pushcfunction(L, l_gpio_get_level);
    lua_setfield(L, -2, "get_level");

    // 注册gpio常量
    lua_pushinteger(L, GPIO_MODE_INPUT);
    lua_setfield(L, -2, "MODE_INPUT");

    lua_pushinteger(L, GPIO_MODE_OUTPUT);
    lua_setfield(L, -2, "MODE_OUTPUT");

    lua_pushinteger(L, GPIO_MODE_INPUT_OUTPUT);
    lua_setfield(L, -2, "MODE_INPUT_OUTPUT");

    lua_pushinteger(L, GPIO_MODE_INPUT_OUTPUT_OD);
    lua_setfield(L, -2, "MODE_INPUT_OUTPUT_OD");

    lua_pushinteger(L, GPIO_MODE_OUTPUT_OD);
    lua_setfield(L, -2, "MODE_OUTPUT_OD");

    lua_setglobal(L, "gpio");

    lua_pushthread(L); // 当前线程作为upvalue
    lua_pushcclosure(L, l_settimeout, 1);
    // lua_register(L, "settimeout", l_settimeout);

    lua_setglobal(L, "settimeout");

    // 创建pwm表
    // lua_newtable(L);

    // // 注册pwm.set_duty函数
    // lua_pushcfunction(L, l_pwm_set_duty);
    // lua_setfield(L, -2, "set_duty");

    // lua_setglobal(L, "pwm");

    // 注册read_adc函数
    lua_register(L, "read_adc", l_read_adc);

    // 注册file.read函数
    lua_register(L, "file.read", l_file_read);

    // 注册file.write函数
    lua_register(L, "file.write", l_file_write);

    lua_register(L, "read_sensor", l_read_sensor);
    // 初始化ADC
    adc1_config_width(ADC_WIDTH_BIT_13);
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_0); // 根据需要配置ADC通道
}
