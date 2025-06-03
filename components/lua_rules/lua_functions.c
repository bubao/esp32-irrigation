#include "lua_functions.h"
#include "lua_bindings.h"

#include "dirent.h"
#include "driver/adc.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include "esp_spi_flash.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <dirent.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static const char* TAG = "lua_system";
LuaCoroutine coroutines[MAX_COROUTINES] = { 0 }; // 这里定义变量
int coroutine_count = 0;
#define MAX_PATH_LEN 256

lua_State* global_L = NULL; // 主 Lua 状态机

// LittleFS 文件系统初始化
void init_littlefs()
{
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/assets",
        .partition_label = "assets",
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount or format filesystem: %s", esp_err_to_name(ret));
    } else {
        size_t total = 0, used = 0;
        esp_littlefs_info("assets", &total, &used);
        ESP_LOGI(TAG, "LittleFS mounted: total=%d, used=%d", total, used);
    }
}

// 清空协程数组
static void clear_coroutines()
{
    for (int i = 0; i < MAX_COROUTINES; i++) {
        coroutines[i].co = NULL;
    }
    coroutine_count = 0;
}

// 加载单个脚本为协程，调用 setup 并加入数组
// 加载单个脚本为协程，调用 setup，并把 loop 函数作为协程入口，加入数组
static bool load_lua_rule_with_setup(lua_State* L, const char* path)
{
    lua_State* co = lua_newthread(L);
    if (!co) {
        ESP_LOGE(TAG, "Failed to create Lua thread for %s", path);
        return false;
    }

    // 保持对主线程的引用
    lua_getglobal(co, "coroutine");
    if (lua_isnil(co, -1)) {
        ESP_LOGE(TAG, "coroutine module not found in global environment of %s", path);
        lua_pop(L, 2); // 弹出 coroutine 和 function 参数
        return false;
    }

    // 尝试加载文件
    if (luaL_loadfile(co, path) != LUA_OK) {
        ESP_LOGE(TAG, "Failed to load %s: %s", path, lua_tostring(co, -1));
        lua_pop(L, 2); // 弹出 coroutine 和 function 参数
        return false;
    }

    // 先执行 Lua 文件，让 loop/setup 注册到全局
    if (lua_pcall(co, 0, 0, 0) != LUA_OK) {
        ESP_LOGE(TAG, "Failed to run %s: %s", path, lua_tostring(co, -1));
        lua_pop(L, 2); // 弹出 coroutine 和 function 参数
        return false;
    }

    // 调用 setup() 函数（如果存在）
    lua_getglobal(co, "setup");
    if (lua_isfunction(co, -1)) {
        if (lua_pcall(co, 0, 0, 0) != LUA_OK) {
            ESP_LOGE(TAG, "setup error in %s: %s", path, lua_tostring(co, -1));
            lua_pop(L, 2); // 弹出 coroutine 和 function 参数
            return false;
        }
    }

    // 注册 settimeout/delay，保证 upvalue 是协程自身
    lua_pushthread(co);
    lua_pushcclosure(co, l_settimeout, 1);
    lua_setglobal(co, "settimeout");
    lua_pushcfunction(co, l_delay);
    lua_setglobal(co, "delay");

    // 准备 loop() 函数作为协程的入口
    lua_getglobal(co, "loop");
    if (!lua_isfunction(co, -1)) {
        ESP_LOGW(TAG, "loop() function not found in %s, skip coroutine", path);
        lua_pop(L, 2); // 弹出 coroutine 和 function 参数
        return true; // 不报错，直接跳过
    }

    // 注意，此时 loop() 在栈顶，下一次 resume 执行它
    if (coroutine_count < MAX_COROUTINES) {
        coroutines[coroutine_count].co = co;
        coroutines[coroutine_count].is_active = 1;
        coroutines[coroutine_count].wake_up_time_us = 0;
        coroutine_count++;
        ESP_LOGI(TAG, "Loaded Lua rule: %s", path);
        return true;
    } else {
        ESP_LOGW(TAG, "Max coroutines reached, skipping: %s", path);
        return false;
    }
}

static void load_lua_rules(lua_State* L, const char* dir)
{
    DIR* d = opendir(dir);
    if (!d) {
        ESP_LOGE(TAG, "Failed to open directory: %s", dir);
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(d)) != NULL) {
        if (strstr(entry->d_name, ".lua")) {
            char filepath[MAX_PATH_LEN] = { 0 };
            // 先复制目录
            strlcpy(filepath, dir, sizeof(filepath));

            // 添加结尾斜杠（若没有）
            size_t len = strlen(filepath);
            if (filepath[len - 1] != '/') {
                strlcat(filepath, "/", sizeof(filepath));
            }

            // 添加文件名
            strlcat(filepath, entry->d_name, sizeof(filepath));

            if (strlen(filepath) >= MAX_PATH_LEN - 1) {
                ESP_LOGW(TAG, "路径过长，跳过: %s/%s", dir, entry->d_name);
                continue;
            }
            ESP_LOGI(TAG, "加载 Lua 文件: %s", filepath);
            load_lua_rule_with_setup(L, filepath);
        }
    }
    closedir(d);
}

static void lua_coroutine_task(void* pvParam)
{
    (void)pvParam;

    while (1) {
        for (int i = 0; i < coroutine_count; i++) {
            lua_State* co = coroutines[i].co; // 取出协程指针
            if (!co)
                continue;

            int nresults = 0;
            int status = lua_resume(co, NULL, 0, &nresults);
            // 自动循环loop: 如果返回OK，尝试再次调用loop，直到yield或出错
            while (status == LUA_OK) {
                lua_getglobal(co, "loop");
                if (!lua_isfunction(co, -1)) {
                    ESP_LOGI(TAG, "Coroutine %d loop finished, removing", i);
                    lua_pop(co, 1); // 清理栈顶，防止栈溢出
                    coroutines[i].co = NULL;
                    coroutines[i].is_active = 0;
                    break;
                }
                status = lua_resume(co, NULL, 0, &nresults);
            }
            if (status == LUA_YIELD) {
                // 正常挂起，等待下一次resume
            } else if (status != LUA_OK) {
                ESP_LOGE(TAG, "Coroutine %d error: %s", i, lua_tostring(co, -1));
                lua_pop(co, 1);
                coroutines[i].co = NULL;
                coroutines[i].is_active = 0;
            }
        }

        // 清理已结束协程，压缩数组
        int write_index = 0;
        for (int read_index = 0; read_index < coroutine_count; read_index++) {
            if (coroutines[read_index].co != NULL) {
                coroutines[write_index++] = coroutines[read_index];
            }
        }
        coroutine_count = write_index;

        // timer_process(global_L); // 禁止主动调用，只允许 esp_timer 定时调度

        vTaskDelay(pdMS_TO_TICKS(10)); // 10ms 延迟，避免死循环烧CPU
    }
}

static lua_State* init_lua()
{
    lua_State* L = luaL_newstate();
    if (!L) {
        ESP_LOGE(TAG, "Failed to create Lua state");
        return NULL;
    }
    luaL_openlibs(L);

    register_lua_bindings(L);

    return L;
}

void start_lua_system()
{
    if (global_L) {
        ESP_LOGW(TAG, "Lua system already started");
        return;
    }

    ESP_LOGI(TAG, "Starting Lua system...");

    global_L = init_lua();
    if (!global_L) {
        ESP_LOGE(TAG, "Failed to initialize Lua");
        return;
    }

    clear_coroutines();
    load_lua_rules(global_L, "/assets");

    // 创建协程调度任务
    xTaskCreate(lua_coroutine_task, "lua_coroutine_task", 8192, NULL, 5, NULL);

    ESP_LOGI(TAG, "Lua system started");
}
