#ifndef BLE_PROVISIONING_H
#define BLE_PROVISIONING_H

#include <string>
#include <functional>

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
};

#endif // BLE_PROVISIONING_H

