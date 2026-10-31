#pragma once

#include <atomic>
#include <cstdint>
#include <memory>

#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "scale/Driver.hpp"
#include "scale/DriverRegistry.hpp"

namespace scale {

/**
 * BLE central that finds a supported scale, connects, subscribes to its notify
 * characteristic and hands bytes to the matching Driver.
 *
 * NimBLE callbacks only record events; poll() (app task) does the work, so
 * drivers and listeners never run on the BLE host task.
 */
class ScaleClient : public Transport, NimBLEScanCallbacks, NimBLEClientCallbacks {
public:
    explicit ScaleClient(const DriverRegistry& registry);
    ~ScaleClient() override;

    ScaleClient(const ScaleClient&) = delete;
    ScaleClient& operator=(const ScaleClient&) = delete;

    void setListener(Listener* listener) { listener_ = listener; }

    void begin();
    void poll();

    bool isConnected() const { return driver_ != nullptr; }
    bool send(Command command);

    bool write(std::span<const uint8_t> data) override;
    std::optional<std::vector<uint8_t>> read(const char* characteristicUuid) override;

private:
    enum class State { Scanning, Connecting, Connected };

    struct Notification {
        uint8_t length;
        uint8_t data[64];
    };

    // NimBLEScanCallbacks (BLE host task)
    void onResult(const NimBLEAdvertisedDevice* device) override;
    void onScanEnd(const NimBLEScanResults& results, int reason) override;
    // NimBLEClientCallbacks (BLE host task)
    void onDisconnect(NimBLEClient* client, int reason) override;

    void startScan();
    void connect();
    bool subscribe();
    void handleDisconnect();

    const DriverRegistry& registry_;
    Listener* listener_ = nullptr;
    State state_ = State::Scanning;

    NimBLEClient* client_ = nullptr;
    NimBLERemoteService* service_ = nullptr;
    NimBLERemoteCharacteristic* writeChar_ = nullptr;
    std::unique_ptr<Driver> driver_;
    uint32_t lastTickMs_ = 0;

    // Handed from host task to app task.
    std::atomic<bool> candidateFound_{false};
    std::atomic<bool> disconnected_{false};
    std::atomic<bool> scanEnded_{false};
    NimBLEAddress candidateAddress_;
    const DriverRegistry::Entry* candidateEntry_ = nullptr;
    QueueHandle_t notifications_;
};

} // namespace scale
