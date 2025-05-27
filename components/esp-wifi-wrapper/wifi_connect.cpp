#include "wifi_connect.h"

#include "ssid_manager.h"
#include "wifi_configuration_ap.h"
#include "wifi_station.h"
#include "esp_log.h"

static const char* TAG = "wifi-connect";

extern "C" wifi_connect_mode_t wifi_connect(void)
{
    auto& ssid_list = SsidManager::GetInstance().GetSsidList();
	// Check if there are any saved SSIDs
	ESP_LOGI(TAG, "Checking saved SSIDs, count: %zu", ssid_list.size());
	if (ssid_list.size() > 0) {
		ESP_LOGI(TAG, "Saved SSIDs found: %zu", ssid_list.size());
		for (const auto& item : ssid_list) {
			ESP_LOGI(TAG, "SSID: %s, Password: %s", item.ssid.c_str(), item.password.c_str());
		}
	} else {
		ESP_LOGI(TAG, "No saved SSIDs found.");
	}
    if (ssid_list.empty() == false) {
        ESP_LOGI(TAG, "SSID found, entering STA mode.");
        WifiStation::GetInstance().Start();
		// Wait for connection to be established
		WifiStation::GetInstance().WaitForConnected(10000); // 30 seconds timeout
		if (WifiStation::GetInstance().IsConnected()) {
			return WIFI_CONNECT_MODE_STA;
		}
		WifiStation::GetInstance().Stop();
    }
	
	ESP_LOGI(TAG, "No saved SSID, entering AP mode.");
    auto& ap = WifiConfigurationAp::GetInstance();
    ap.SetSsidPrefix("ESP32");
    ap.Start();
    return WIFI_CONNECT_MODE_AP;
}
