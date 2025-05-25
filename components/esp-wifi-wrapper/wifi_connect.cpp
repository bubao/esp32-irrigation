#include "wifi_connect.h"

#include "ssid_manager.h"
#include "wifi_configuration_ap.h"
#include "wifi_station.h"
#include "esp_log.h"

static const char* TAG = "wifi-connect";

extern "C" wifi_connect_mode_t wifi_connect(void)
{
    auto& ssid_list = SsidManager::GetInstance().GetSsidList();
    if (ssid_list.empty()) {
        ESP_LOGI(TAG, "No saved SSID, entering AP mode.");
        auto& ap = WifiConfigurationAp::GetInstance();
        ap.SetSsidPrefix("ESP32");
        ap.Start();
        return WIFI_CONNECT_MODE_AP;
    } else {
        ESP_LOGI(TAG, "SSID found, entering STA mode.");
        WifiStation::GetInstance().Start();
        return WIFI_CONNECT_MODE_STA;
    }
}
