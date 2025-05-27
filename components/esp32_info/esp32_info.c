#include "esp32_info.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h" // 添加 esp_timer.h
#include "spi_flash_mmap.h" // 替换 esp_spi_flash.h
#include "time_sync.h"
#include <time.h>

const char* get_chip_model_name(int chip_model)
{
    switch (chip_model) {
    case CHIP_ESP32:
        return "ESP32";
    case CHIP_ESP32S2:
        return "ESP32-S2";
    case CHIP_ESP32S3:
        return "ESP32-S3";
    case CHIP_ESP32C3:
        return "ESP32-C3";
    case CHIP_ESP32C2:
        return "ESP32-C2";
    case CHIP_ESP32C6:
        return "ESP32-C6";
    case CHIP_ESP32H2:
        return "ESP32-H2";
    case CHIP_ESP32P4:
        return "ESP32-P4";
    case CHIP_ESP32C61:
        return "ESP32-C61";
    case CHIP_ESP32C5:
        return "ESP32-C5";
    case CHIP_POSIX_LINUX:
        return "POSIX_LINUX";
    default:
        return "Unknown";
    }
}

void get_esp32_info(esp32_info_t* info)
{
    esp_chip_info_t chip_info;
    uint32_t flash_size;

    // 获取芯片信息
    esp_chip_info(&chip_info);

    // 解析芯片信息
    info->chip_model = chip_info.model;
    info->major_rev = chip_info.revision / 100;
    info->minor_rev = chip_info.revision % 100;
    info->cores = chip_info.cores;

    // 检查支持的协议
    info->wifi_bgn = (chip_info.features & CHIP_FEATURE_WIFI_BGN) != 0;
    info->bt = (chip_info.features & CHIP_FEATURE_BT) != 0;
    info->ble = (chip_info.features & CHIP_FEATURE_BLE) != 0;
    info->ieee802154 = (chip_info.features & CHIP_FEATURE_IEEE802154) != 0;

    // 获取闪存大小
    if (esp_flash_get_size(NULL, &flash_size) == ESP_OK) {
        info->flash_size_mb = flash_size / (1024 * 1024); // 转换为MB
    } else {
        info->flash_size_mb = 0;
    }

    // 获取堆内存信息
    info->total_heap_size = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    info->free_heap_size = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    info->minimum_free_heap_size = esp_get_minimum_free_heap_size();
}

void print_esp32_info(const esp32_info_t* info)
{
    // const char* chip_model = get_chip_model_name(info->chip_model);
    // ESP_LOGI("INFO", "Chip model: %s", chip_model);
    // ESP_LOGI("INFO", "Chip revision: %d.%d", info->major_rev, info->minor_rev);
    // ESP_LOGI("INFO", "Number of cores: %d", info->cores);
    // ESP_LOGI("INFO", "Supported protocols:");
    // if (info->wifi_bgn) {
    //     ESP_LOGI("INFO", " - Wi-Fi (802.11 b/g/n)");
    // }
    // if (info->bt) {
    //     ESP_LOGI("INFO", " - Bluetooth (BT)");
    // }
    // if (info->ble) {
    //     ESP_LOGI("INFO", " - Bluetooth Low Energy (BLE)");
    // }
    // if (info->ieee802154) {
    //     ESP_LOGI("INFO", " - IEEE 802.15.4");
    // }
    // ESP_LOGI("INFO", "Flash size: %" PRIu32 " MB", info->flash_size_mb);
    // ESP_LOGI("INFO", "Total heap size: %" PRIu32 " KB", info->total_heap_size / 1024);
    // ESP_LOGI("INFO", "Free heap size: %" PRIu32 " KB", info->free_heap_size / 1024);
    // ESP_LOGI("INFO", "Minimum free heap size: %" PRIu32 " KB", info->minimum_free_heap_size / 1024);
    bool time_synced = is_time_synced();
    if (time_synced) {
        // 打印当前时间
        time_t now;
        time(&now);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        ESP_LOGI("INFO", "Current time: %s", asctime(&timeinfo));
    }
}