#include "ble_provisioning.h"

#include <esp_log.h>
#include <esp_mac.h>
#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_ble.h>

#include <cstring>

static const char* TAG = "PROV";

BleProvisioning& BleProvisioning::GetInstance() {
    static BleProvisioning instance;
    return instance;
}

static std::string MakeDeviceName(const std::string& prefix) {
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char suffix[7];
    // last 3 bytes
    snprintf(suffix, sizeof(suffix), "%02X%02X%02X", mac[3], mac[4], mac[5]);
    return prefix + suffix;
}

void BleProvisioning::Start(const std::string& namePrefix, const std::string& pop,
                            std::function<void(const std::string& ssid)> onConnecting,
                            std::function<void(const std::string& ssid)> onSuccess,
                            std::function<void(const std::string& reason)> onFailure,
                            uint32_t timeout_minutes) {
    if (running_) {
        ESP_LOGW(TAG, "Provisioning already running");
        return;
    }

    wifi_prov_mgr_config_t prov_cfg = {};
    prov_cfg.scheme = wifi_prov_scheme_ble;
    prov_cfg.scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM;
    ESP_ERROR_CHECK(wifi_prov_mgr_init(prov_cfg));

    bool provisioned = false;
    wifi_prov_mgr_is_provisioned(&provisioned);
    if (!provisioned) {
        std::string device_name = MakeDeviceName(namePrefix);
        ESP_LOGI(TAG, "Starting BLE provisioning, name: %s", device_name.c_str());
        wifi_prov_security_t security = WIFI_PROV_SECURITY_1;
        const char* service_name = device_name.c_str();
        const char* service_key = NULL; // not used
        const char* pop_str = pop.c_str();
        wifi_prov_mgr_endpoint_create("custom-data");
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(security, (const void*)pop_str, service_name, service_key));
    } else {
        ESP_LOGI(TAG, "Already provisioned, skip start");
        wifi_prov_mgr_deinit();
        return;
    }

    running_ = true;

    // We do not register custom events; rely on default workflow
    // User app monitors Wi-Fi station events to call onConnecting/onSuccess/onFailure

    // Timeout handling (simple): create a timer task if needed by caller
    // For minimal integration, leave to caller to stop after timeout
}

void BleProvisioning::Stop() {
    if (!running_) return;
    ESP_LOGI(TAG, "Stopping BLE provisioning");
    wifi_prov_mgr_stop_provisioning();
    wifi_prov_mgr_deinit();
    running_ = false;
}

