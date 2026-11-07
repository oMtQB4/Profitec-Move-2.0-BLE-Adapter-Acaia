#pragma once

#include <atomic>
#include <cstdint>
#include <string>

#include <NimBLEDevice.h>

#include "acaia/AcaiaProtocol.hpp"
#include "common/PeerAddress.hpp"

namespace acaia {

/** Receives commands the coffee machine sends to the emulated scale. */
class CommandSink {
public:
    virtual ~CommandSink() = default;
    /** Runs on the BLE host task: enqueue, don't block. */
    virtual void onAcaiaCommand(Command command) = 0;
};

/**
 * GATT server that looks like an Acaia Lunar (2021) to a client. Answers the
 * protocol handshake itself and forwards actionable commands to the sink.
 */
class AcaiaPeripheral : NimBLEServerCallbacks, NimBLECharacteristicCallbacks {
public:
    explicit AcaiaPeripheral(std::string advertisedName);

    AcaiaPeripheral(const AcaiaPeripheral&) = delete;
    AcaiaPeripheral& operator=(const AcaiaPeripheral&) = delete;

    void setCommandSink(CommandSink* sink) { sink_ = sink; }
    void setBatteryPercent(uint8_t percent) { batteryPct_ = percent; }

    /**
     * Adds the Acaia and Device Information services and the advertising data.
     * The caller starts the GATT server after adding its own services, so the
     * handles of these two stay the same.
     */
    void begin();
    void startAdvertising();
    void stopAdvertising();
    void disconnectClient();

    bool isClientConnected() const { return connHandle_.load() != kNoConnection; }
    /** Counts connections; a change tells a new client from the one seen before. */
    uint32_t connectionCount() const { return connections_.load(); }
    /** Identity address of the current (or last) client. */
    PeerAddress clientAddress() const { return unpack(peer_.load()); }
    /** The client subscribed to the weight notifications. */
    bool isSubscribed() const { return subscribed_.load(); }
    /** The client wrote an Acaia frame since it connected. */
    bool clientWrote() const { return wrote_.load(); }

    void sendWeight(int32_t weightDg);

private:
    static constexpr uint16_t kNoConnection = 0xFFFF;

    static uint64_t pack(const PeerAddress& address);
    static PeerAddress unpack(uint64_t packed);

    // NimBLEServerCallbacks
    void onConnect(NimBLEServer* server, NimBLEConnInfo& connInfo) override;
    void onDisconnect(NimBLEServer* server, NimBLEConnInfo& connInfo, int reason) override;
    // NimBLECharacteristicCallbacks
    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo) override;
    void onRead(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo) override;
    void onSubscribe(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo, uint16_t subValue) override;

    void addDeviceInformationService();
    void handle(Command command);
    void notify(const Frame& frame);

    std::string advertisedName_;
    CommandSink* sink_ = nullptr;
    NimBLEServer* server_ = nullptr;
    NimBLECharacteristic* notifyChar_ = nullptr;
    std::atomic<bool> advertisingWanted_{false};
    std::atomic<uint16_t> connHandle_{kNoConnection};
    std::atomic<bool> subscribed_{false};
    std::atomic<bool> wrote_{false};
    std::atomic<uint32_t> connections_{0};
    std::atomic<uint64_t> peer_{0}; // PeerAddress: 6 address bytes, type in byte 6
    std::atomic<uint8_t> batteryPct_{100};
};

} // namespace acaia
