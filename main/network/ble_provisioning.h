#ifndef BLE_PROVISIONING_H
#define BLE_PROVISIONING_H

#include <string>
#include <functional>
#include <esp_event.h>
#include <esp_timer.h>

class BleProvisioning {
public:
    static BleProvisioning& GetInstance();

    // Start BLE provisioning; namePrefix should be "Liuliu-" and module will append MAC suffix
    // pop is fixed to "123456" per requirement
    void Start(const std::string& namePrefix, const std::string& pop,
               std::function<void(const std::string& ssid)> onConnecting,
               std::function<void(const std::string& ssid)> onSuccess,
               std::function<void(const std::string& reason)> onFailure,
               uint32_t timeout_minutes = 10);

    void Stop();
    bool IsRunning() const { return running_; }

private:
    BleProvisioning() = default;
    ~BleProvisioning() = default;

    bool running_ = false;

    std::function<void(const std::string&)> on_connecting_;
    std::function<void(const std::string&)> on_success_;
    std::function<void(const std::string&)> on_failure_;

    esp_timer_handle_t timeout_timer_ = nullptr;

    static void EventHandler(void* handler_args, esp_event_base_t event_base, int32_t event_id, void* event_data);
    void RegisterEventHandlers();
    void UnregisterEventHandlers();
};

#endif // BLE_PROVISIONING_H

