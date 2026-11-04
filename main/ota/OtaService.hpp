#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "ota/OtaSession.hpp"

namespace ota {

/**
 * GATT side of the firmware update (protocol in OtaProtocol.hpp).
 *
 * The service is part of the GATT table in every mode, so the table a phone
 * caches never changes; outside update mode it refuses Begin. NimBLE
 * callbacks only queue writes; poll() feeds them to the session on the app
 * task, where the flash writes happen.
 */
class OtaService : NimBLECharacteristicCallbacks, Notifier {
public:
    OtaService(FlashTarget& flash, std::string firmwareVersion);
    ~OtaService() override;

    OtaService(const OtaService&) = delete;
    OtaService& operator=(const OtaService&) = delete;

    /** Adds the service. Call before NimBLEServer::start(), after the Acaia services (their handles stay put). */
    void registerService(NimBLEServer& server);

    /** Update mode: accept firmware and advertise as `name` with the update service UUID. */
    void enterUpdateMode(const std::string& name);

    /** Processes queued writes for up to `waitMs`. Call from the app task. */
    void poll(uint32_t nowMs, uint32_t waitMs = 0);

    bool isTransferring() const { return session_.state() == OtaSession::State::Receiving; }
    /** The new image is verified and set as boot target. */
    bool isDone() const { return session_.state() == OtaSession::State::Done; }
    /** Last time a client wrote to the service, in ms since boot. */
    uint32_t lastActivityMs() const { return lastActivityMs_; }

private:
    static constexpr uint16_t kWindow = 16;

    struct Message {
        enum class Kind : uint8_t { Control, Data } kind;
        uint16_t connHandle;
        uint16_t chunk;
        uint16_t length;
        int64_t receivedUs; // when NimBLE delivered the write
        uint8_t data[kMaxAttributeLength];
    };

    // NimBLECharacteristicCallbacks (BLE host task)
    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo) override;
    void onRead(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo) override;
    // Notifier (app task)
    void notify(std::span<const uint8_t> frame) override;

    void handle(const Message& message, uint32_t nowMs);
    void traceData(const Message& message);
    void logLink(uint16_t connHandle);
    void flushNotifications();
    void keepAdvertising();

    std::string firmwareVersion_;
    uint32_t capacity_;
    OtaSession session_;
    QueueHandle_t queue_;
    Message scratch_{};  // BLE host task
    Message received_{}; // app task
    NimBLECharacteristic* control_ = nullptr;
    std::atomic<bool> updateMode_{false};
    std::atomic<bool> transferring_{false};
    std::atomic<bool> overflow_{false};
    uint16_t sessionConn_ = BLE_HS_CONN_HANDLE_NONE;
    uint16_t loggedConn_ = BLE_HS_CONN_HANDLE_NONE;
    uint32_t lastActivityMs_ = 0;
    int64_t startedUs_ = 0; // transfer statistics
    int64_t busyUs_ = 0;
    uint32_t dataPackets_ = 0; // data packets since Begin, for the timing log
    int64_t lastDataUs_ = 0;
    std::deque<std::vector<uint8_t>> pending_; // notifications NimBLE had no buffer for yet
};

} // namespace ota
