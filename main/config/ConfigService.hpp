#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "config/ConfigProtocol.hpp"
#include "config/Settings.hpp"

namespace config {

/**
 * GATT side of the configurator (protocol in ConfigProtocol.hpp).
 *
 * NimBLE callbacks only queue writes; poll() applies them on the app task,
 * saves the settings and reports what changed. Status updates come from the
 * app task too. Reads are served from the characteristic values.
 */
class ConfigService : NimBLECharacteristicCallbacks {
public:
    struct Events {
        std::optional<Settings> settings; ///< changed settings, already saved
        bool forgetMachine = false;
    };

    explicit ConfigService(const Settings& settings);
    ~ConfigService() override;

    ConfigService(const ConfigService&) = delete;
    ConfigService& operator=(const ConfigService&) = delete;

    /** Adds the service. Call before NimBLEServer::start(), after the update service. */
    void registerService(NimBLEServer& server);

    /** Applies queued writes. Call from the app task. */
    Events poll();

    /** Publishes the adapter status; notifies subscribers when it changed. Call from the app task. */
    void setStatus(const Status& status);

    const Settings& settings() const { return settings_; }

private:
    static constexpr size_t kMaxWrite = 64;

    struct Message {
        enum class Kind : uint8_t { Settings, Control } kind;
        uint8_t length;
        uint8_t data[kMaxWrite];
    };

    // NimBLECharacteristicCallbacks (BLE host task)
    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo) override;

    void publishSettings();

    Settings settings_;
    std::optional<Status> status_;
    QueueHandle_t queue_;
    NimBLECharacteristic* settingsChar_ = nullptr;
    NimBLECharacteristic* statusChar_ = nullptr;
    NimBLECharacteristic* controlChar_ = nullptr;
};

} // namespace config
