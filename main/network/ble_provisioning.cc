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

    on_connecting_ = onConnecting;
    on_success_ = onSuccess;
    on_failure_ = onFailure;

    wifi_prov_mgr_config_t prov_cfg = {};
    prov_cfg.scheme = wifi_prov_scheme_ble;
    prov_cfg.scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM;
    ESP_ERROR_CHECK(wifi_prov_mgr_init(prov_cfg));

    bool provisioned = false;
    wifi_prov_mgr_is_provisioned(&provisioned);
    if (!provisioned) {
        std::string device_name = MakeDeviceName(namePrefix);
        ESP_LOGI(TAG, "Starting BLE provisioning, name: %s", device_name.c_str());
        // Explicitly set BLE device name for scheme
        wifi_prov_scheme_ble_set_device_name(device_name.c_str());
        wifi_prov_security_t security = WIFI_PROV_SECURITY_1;
        const char* service_name = device_name.c_str();
        const char* service_key = NULL; // not used
        const char* pop_str = pop.c_str();
        wifi_prov_mgr_endpoint_create("custom-data");
        RegisterEventHandlers();
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(security, (const void*)pop_str, service_name, service_key));
    } else {
        ESP_LOGI(TAG, "Already provisioned, skip start");
        wifi_prov_mgr_deinit();
        return;
    }

    running_ = true;
    // Timeout handling
    if (timeout_minutes > 0) {
        esp_timer_create_args_t targs = {
            .callback = [](void* arg) {
                auto self = static_cast<BleProvisioning*>(arg);
                ESP_LOGW(TAG, "Provisioning timeout");
                if (self->on_failure_) self->on_failure_("timeout");
                self->Stop();
            },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "prov_timeout"
        };
        ESP_ERROR_CHECK(esp_timer_create(&targs, &timeout_timer_));
        ESP_ERROR_CHECK(esp_timer_start_once(timeout_timer_, (uint64_t)timeout_minutes * 60ULL * 1000000ULL));
    }
}

void BleProvisioning::Stop() {
    if (!running_) return;
    ESP_LOGI(TAG, "Stopping BLE provisioning");
    wifi_prov_mgr_stop_provisioning();
    UnregisterEventHandlers();
    wifi_prov_mgr_deinit();
    if (timeout_timer_) {
        esp_timer_stop(timeout_timer_);
        esp_timer_delete(timeout_timer_);
        timeout_timer_ = nullptr;
    }
    running_ = false;
}

void BleProvisioning::EventHandler(void* handler_args, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    auto self = static_cast<BleProvisioning*>(handler_args);
    if (event_base == WIFI_PROV_EVENT) {
        switch (event_id) {
        case WIFI_PROV_START:
            ESP_LOGI(TAG, "Provisioning started");
            break;
        case WIFI_PROV_CRED_RECV: {
            auto* evt = (wifi_prov_sta_conn_info_t*)event_data;
            if (evt && evt->ssid) {
                ESP_LOGI(TAG, "Credentials received for ssid %s", evt->ssid);
                if (self->on_connecting_) self->on_connecting_(evt->ssid);
            }
            break;
        }
        case WIFI_PROV_CRED_SUCCESS: {
            auto* evt = (wifi_prov_sta_conn_info_t*)event_data;
            ESP_LOGI(TAG, "Provisioning success");
            if (self->on_success_) self->on_success_(evt && evt->ssid ? evt->ssid : "");
            break;
        }
        case WIFI_PROV_CRED_FAIL: {
            auto* fail = (wifi_prov_sta_fail_reason_t*)event_data;
            const char* reason = "unknown";
            if (fail) {
                reason = (*fail == WIFI_PROV_STA_AUTH_ERROR) ? "auth_error" : ((*fail == WIFI_PROV_STA_AP_NOT_FOUND) ? "ap_not_found" : "fail");
            }
            ESP_LOGW(TAG, "Provisioning failed: %s", reason);
            if (self->on_failure_) self->on_failure_(reason);
            break;
        }
        case WIFI_PROV_END:
            ESP_LOGI(TAG, "Provisioning end");
            self->Stop();
            break;
        default:
            break;
        }
    }
}

void BleProvisioning::RegisterEventHandlers() {
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &BleProvisioning::EventHandler, this));
}

void BleProvisioning::UnregisterEventHandlers() {
    esp_event_handler_unregister(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &BleProvisioning::EventHandler);
}

