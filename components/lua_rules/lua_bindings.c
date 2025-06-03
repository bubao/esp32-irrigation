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

timer_task_t* timer_list = NULL; // 不要 static，供多文件共享

// 定时器句柄
static esp_timer_handle_t lua_timer_handle = NULL;

// timer_process 的 esp_timer 回调
static void lua_timer_callback(void* arg)
{
    extern lua_State* global_L;
    timer_process(global_L);
}

LuaCoroutine* get_current_coroutine(lua_State* L)
{
    for (int i = 0; i < MAX_COROUTINES; i++) {
        if (coroutines[i].co == L) {
            return &coroutines[i];
        }
    }
    return NULL;
}
int l_settimeout(lua_State* L)
{
    int ms = luaL_checkinteger(L, 1);
    int64_t now_us = esp_timer_get_time();

    // ESP_LOGI("TIMER_DEBUG", "l_settimeout: &timer_list=%p, timer_list=%p", &timer_list, timer_list);

    // 获取当前协程
    lua_State* co = lua_tothread(L, lua_upvalueindex(1)); // 取upvalue的协程线程
    if (!co) {
        co = L;
    }
    // ESP_LOGI("LUA", "l_settimeout called, co=%p, ms=%d", co, ms);

    // 防止主线程调用 settimeout
    extern lua_State* global_L;
    if (co == global_L) {
        ESP_LOGE("SETTIMEOUT", "settimeout/delay cannot be called from main thread");
        return luaL_error(L, "settimeout/delay cannot be called from main thread");
    }

    // 插入 timer_list 前，移除所有已有的同协程 timer_task，避免重复定时
    timer_task_t** p = &timer_list;
    while (*p) {
        if ((*p)->co == co) {
            timer_task_t* to_free = *p;
            *p = (*p)->next;
            free(to_free);
        } else {
            p = &(*p)->next;
        }
    }
    // 创建定时任务节点
    timer_task_t* task = malloc(sizeof(timer_task_t));
    if (!task) {
        return luaL_error(L, "Out of memory");
    }
    task->co = co;
    task->wakeup_time_ms = (now_us / 1000) + ms; // 不做 int 强转
    ESP_LOGI("SETTIMEOUT", "now_ms=%lld, delay=%d, wakeup_time_ms=%lld", now_us / 1000, ms, task->wakeup_time_ms);
    task->next = NULL;

    // 按升序插入链表
    if (!timer_list || timer_list->wakeup_time_ms > task->wakeup_time_ms) {
        task->next = timer_list;
        timer_list = task;
    } else {
        timer_task_t* prev = timer_list;
        while (prev->next && prev->next->wakeup_time_ms <= task->wakeup_time_ms) {
            prev = prev->next;
        }
        task->next = prev->next;
        prev->next = task;
    }

    // ESP_LOGI("TIMER_DEBUG", "l_settimeout after insert: &timer_list=%p, timer_list=%p", &timer_list, timer_list);

    // 挂起当前协程，返回给调用者
    return lua_yield(L, 0);
}

// delay(ms) 函数，内部用settimeout实现，Lua接口改为 delay()
int l_delay(lua_State* L)
{
    // int ms = luaL_checkinteger(L, 1);
    // 这里调用之前的 settimeout 实现逻辑
    // 复用 l_settimeout 逻辑
    return l_settimeout(L);
}

static int l_log(lua_State* L)
{
    const char* msg = luaL_checkstring(L, 1);
    ESP_LOGI("LUA", "%s", msg);
    return 0;
}

// 定时器轮询，唤醒过期协程
// 定时器唤醒函数，放在定时器轮询中调用
void timer_process(lua_State* L)
{
    int64_t now_ms = esp_timer_get_time() / 1000;
    if (timer_list && timer_list->wakeup_time_ms <= now_ms) {
        timer_task_t* curr = timer_list;
        timer_list = curr->next;
        lua_State* co = curr->co;
        if (co) {
            ESP_LOGI("TIMER", "now_ms=%lld, wakeup_time_ms=%lld, delta=%lld", now_ms, curr->wakeup_time_ms, curr->wakeup_time_ms - now_ms);
            for (int i = 0; i < MAX_COROUTINES; i++) {
                if (coroutines[i].co == co) {
                    coroutines[i].is_active = 1;
                    if (lua_status(co) == LUA_YIELD) {
                        int nresults = 0;
                        int status = lua_resume(co, NULL, 0, &nresults);
                        if (status == LUA_YIELD) {
                            ESP_LOGI("TIMER", "Coroutine %d yielded, will be rescheduled", i);
                        } else if (status == LUA_OK) {
                            ESP_LOGW("TIMER", "Coroutine %d finished loop()", i);
                        } else {
                            const char* err = lua_tostring(co, -1);
                            ESP_LOGE("TIMER", "Coroutine %d error: %s", i, err ? err : "<unknown error>");
                            lua_pop(co, 1);
                        }
                    } else {
                        ESP_LOGW("TIMER", "Coroutine %d is not yieldable (status=%d), skip", i, lua_status(co));
                    }
                    break;
                }
            }
        }
        free(curr);
        // 跳过 timer_list 里所有 co 相同的 timer_task，防止同一tick多次唤醒
        while (timer_list && timer_list->co == co) {
            timer_task_t* dup = timer_list;
            timer_list = dup->next;
            free(dup);
        }
    }
}

// 启动所有协程的 loop()
void start_all_loops(lua_State* L)
{
    for (int i = 0; i < MAX_COROUTINES; i++) {
        if (coroutines[i].co) {
            lua_State* co = coroutines[i].co;
            lua_getglobal(co, "loop");
            if (lua_isfunction(co, -1)) {
                int nresults = 0;
                int status = lua_resume(co, NULL, 0, &nresults);
                if (status == LUA_YIELD) {
                    ESP_LOGI("LUA", "Coroutine %d started and yielded", i);
                } else if (status == LUA_OK) {
                    ESP_LOGW("LUA", "Coroutine %d finished loop() immediately", i);
                } else {
                    const char* err = lua_tostring(co, -1);
                    ESP_LOGE("LUA", "Coroutine %d error: %s", i, err ? err : "<unknown error>");
                    lua_pop(co, 1);
                }
            } else {
                lua_pop(co, 1); // pop non-function
            }
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

    // lua_pushthread(L); // 当前协程线程
    // lua_pushcclosure(L, l_settimeout, 1);
    // lua_setglobal(L, "settimeout");

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

    lua_register(L, "log", l_log);

    // lua_pushcfunction(L, l_delay);
    // lua_setglobal(L, "delay");

    // start_loop(L);
    // 初始化ADC
    adc1_config_width(ADC_WIDTH_BIT_13);
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_0); // 根据需要配置ADC通道

    // 启动 esp_timer 定时器，每 10ms 调用一次 timer_process
    if (!lua_timer_handle) {
        const esp_timer_create_args_t timer_args = {
            .callback = &lua_timer_callback,
            .name = "lua_timer"
        };
        esp_timer_create(&timer_args, &lua_timer_handle);
        esp_timer_start_periodic(lua_timer_handle, 10 * 1000); // 10ms
    }

    // 启动 loop 协程，只需启动一次
    // start_loop(L);
}
