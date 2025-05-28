// main/lua_functions.c
#include "lua_functions.h"
#include "dirent.h"
#include "driver/adc.h"
#include "driver/gpio.h"
#include "driver/mcpwm.h"
#include "esp_log.h"
#include "esp_spi_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/gpio.h"
#include "soc/gpio_num.h"
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdio.h>
#include <string.h>

#include "esp_littlefs.h"

static const char* TAG = "lua_functions";

// Lua函数：初始化SPIFFS
void init_spiffs()
{
    ESP_LOGI(TAG, "Initializing File System");

    esp_vfs_littlefs_conf_t conf = {
        .base_path = LUA_FILE_PATH,
        .partition_label = "assets",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    esp_err_t err = esp_vfs_littlefs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount or format filesystem");
    } else {
        ESP_LOGI(TAG, "Filesystem mounted at %s", LUA_FILE_PATH);
    }

    // 这里写一个示例，挂载成功后创建文件，确认文件系统可写
    FILE* f = fopen("/assets/test.txt", "w");
    if (f) {
        fprintf(f, "Hello LittleFS\n");
        fclose(f);
        ESP_LOGI(TAG, "File created successfully");
    } else {
        ESP_LOGE(TAG, "Failed to create file");
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

// Lua函数：延时
static int l_tmr_delay(lua_State* L)
{
    int us = luaL_checkinteger(L, 1);
    esp_rom_delay_us(us);
    return 0;
}

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
int read_sensor()
{
    // 这里可以替换为实际的传感器读取代码
    static int value = 0;
    value = (value + 10) % 100; // 模拟传感器值变化
    return value;
}

lua_State* init_lua()
{
    lua_State* L = luaL_newstate();
    if (L == NULL) {
        ESP_LOGE(TAG, "无法创建Lua状态");
        return NULL;
    }

    luaL_openlibs(L);

    // 创建gpio表
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

    // 初始化ADC
    adc1_config_width(ADC_WIDTH_BIT_13);
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_0); // 根据需要配置ADC通道

    return L;
}

void register_lua_script(lua_State* L, const char* filename)
{
    if (luaL_dofile(L, filename)) {
        ESP_LOGE(TAG, "无法运行脚本: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}
void lua_task(void* pvParameters)
{
    lua_State* L = (lua_State*)pvParameters;

    while (1) {
        if (lua_gettop(L) > 0) {
            int nresults = 0;
            int status = lua_resume(L, NULL, 0, &nresults);
            if (status != LUA_YIELD && status != LUA_OK) {
                ESP_LOGE(TAG, "Lua 脚本执行完毕或出错: %s", lua_tostring(L, -1));
                lua_pop(L, 1); // 弹出错误信息
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void start_lua_task(lua_State* L)
{
    xTaskCreate(lua_task, "lua_task", 8192, L, 5, NULL);
}
#include "esp_log.h"
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_PATH_LEN 256

void register_all_lua_scripts(lua_State* L, const char* directory)
{
    DIR* dir;
    struct dirent* ent;

    if ((dir = opendir(directory)) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_type == DT_REG && strlen(ent->d_name) > 4 && strcmp(ent->d_name + strlen(ent->d_name) - 4, ".lua") == 0) {

                char filepath[MAX_PATH_LEN] = { 0 };

                // 先复制目录
                strlcpy(filepath, directory, sizeof(filepath));

                // 添加结尾斜杠（若没有）
                size_t len = strlen(filepath);
                if (filepath[len - 1] != '/') {
                    strlcat(filepath, "/", sizeof(filepath));
                }

                // 添加文件名
                strlcat(filepath, ent->d_name, sizeof(filepath));

                if (strlen(filepath) >= MAX_PATH_LEN - 1) {
                    ESP_LOGW(TAG, "路径过长，跳过: %s/%s", directory, ent->d_name);
                    continue;
                }

                ESP_LOGI(TAG, "Registering Lua script: %s", filepath);
                register_lua_script(L, filepath);
            }
        }
        closedir(dir);
    } else {
        ESP_LOGE(TAG, "无法打开目录: %s", directory);
    }
}
