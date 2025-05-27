#include "time_sync.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <time.h>

static const char* TAG = "time_sync";

// 只使用 5 个服务器，因为 ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE 最多支持 5 个
static const char* ntp_servers[5] = {
    "ntp.aliyun.com",
    "ntp.ntsc.ac.cn",
    "cn.ntp.org.cn",
    "pool.ntp.org",
    "ntp2.aliyun.com",
};

static volatile bool time_synced = false;

static void time_sync_notification_cb(struct timeval* tv)
{
    setenv("TZ", "CST-8", 1);
    tzset();
    time_synced = true;

    ESP_LOGI(TAG, "Time synchronization event received");
}

void time_sync_start(void)
{
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(
        5, // 不超过 3 个就足够
        ESP_SNTP_SERVER_LIST(
            "cn.pool.ntp.org",
            "ntp.aliyun.com",
            "ntp.ntsc.ac.cn",
            "cn.ntp.org.cn",
            "pool.ntp.org", ));

    config.sync_cb = time_sync_notification_cb;
    config.server_from_dhcp = false; // 建议关闭 DHCP 中的 NTP 覆盖
    config.renew_servers_after_new_IP = true;
    config.ip_event_to_renew = IP_EVENT_STA_GOT_IP;

    esp_netif_sntp_init(&config);
    ESP_LOGI(TAG, "SNTP initialized with multiple servers");
}

void time_sync_task(void* pvParameters)
{
    time_sync_start();

    int retry = 0;

    while (!time_synced) {
        ESP_LOGI(TAG, "Waiting for time sync... (%d), status=%d",
            retry, sntp_get_sync_status());
        if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000)) != ESP_OK) {
            ESP_LOGI(TAG, "Failed to update system time within 10s timeout");
        }
        retry++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (time_synced || sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {

        time_t now;
        time(&now);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        ESP_LOGI(TAG, "Time synchronized: %s", asctime(&timeinfo));
    } else {
        ESP_LOGW(TAG, "Time synchronization failed");
    }

    esp_netif_sntp_deinit(); // 释放资源
    vTaskDelete(NULL);
}

bool is_time_synced()
{
    return time_synced || sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED;
}