#include "acaia/AcaiaPeripheral.hpp"

#include <cstdio>
#include <utility>

#include <esp_log.h>
#include <esp_mac.h>
#include <sdkconfig.h>

#include "acaia/DeviceInfo.hpp"

namespace acaia {

namespace {

constexpr const char* TAG = "acaia";

} // namespace

AcaiaPeripheral::AcaiaPeripheral(std::string advertisedName)
    : advertisedName_(std::move(advertisedName))
{
}

void AcaiaPeripheral::begin()
{
    server_ = NimBLEDevice::createServer();
    server_->setCallbacks(this, false);

    NimBLEService* service = server_->createService(kServiceUuid);
    notifyChar_ = service->createCharacteristic(kNotifyUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    NimBLECharacteristic* writeChar =
        service->createCharacteristic(kWriteUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    NimBLECharacteristic* auxChar = service->createCharacteristic(
        kAuxUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::NOTIFY);
    notifyChar_->setCallbacks(this);
    writeChar->setCallbacks(this);
    auxChar->setCallbacks(this);
    addDeviceInformationService();
    server_->start();
    // start() re-initializes the GAP service, which resets the device name to
    // CONFIG_BT_NIMBLE_SVC_GAP_DEVICE_NAME ("nimble").
    NimBLEDevice::setDeviceName(advertisedName_);

    // A 128-bit UUID and the name don't both fit into 31 bytes: name in the
    // advertisement, service UUID in the scan response.
    NimBLEAdvertisementData advData;
    advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
    advData.setName(advertisedName_);
    NimBLEAdvertisementData scanData;
    scanData.addServiceUUID(NimBLEUUID(kServiceUuid));

    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->setAdvertisementData(advData);
    advertising->setScanResponseData(scanData);
    advertising->enableScanResponse(true);
}

/*
 * Same characteristics as a real Lunar 2021 (aioacaia services.txt). Only the
 * System ID is known to matter; the strings are plausible placeholders.
 */
void AcaiaPeripheral::addDeviceInformationService()
{
    BdAddress address{};
    esp_read_mac(address.data(), ESP_MAC_BT);
    const SystemId systemId = systemIdFromAddress(address);

    char serial[13];
    std::snprintf(serial, sizeof(serial), "%02X%02X%02X%02X%02X%02X", address[0], address[1], address[2],
                  address[3], address[4], address[5]);

    NimBLEService* info = server_->createService("180A");
    auto addReadOnly = [this, info](const char* uuid) {
        NimBLECharacteristic* characteristic = info->createCharacteristic(uuid, NIMBLE_PROPERTY::READ);
        characteristic->setCallbacks(this); // log reads
        return characteristic;
    };
    addReadOnly("2A29")->setValue("acaia");
    addReadOnly("2A24")->setValue("LUNAR");
    addReadOnly("2A25")->setValue(serial);
    addReadOnly("2A27")->setValue("1.0");
    addReadOnly("2A26")->setValue("1.0.016");
    addReadOnly("2A28")->setValue("1.0.016");
    addReadOnly("2A23")->setValue(systemId.data(), systemId.size());
    ESP_LOGI(TAG, "system id %02X %02X %02X %02X %02X %02X %02X %02X", systemId[0], systemId[1], systemId[2],
             systemId[3], systemId[4], systemId[5], systemId[6], systemId[7]);
}

void AcaiaPeripheral::startAdvertising()
{
    advertisingWanted_ = true;
    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    if (!isClientConnected() && !advertising->isAdvertising()) {
        ESP_LOGI(TAG, "advertising as %s", advertisedName_.c_str());
        advertising->start();
    }
}

void AcaiaPeripheral::stopAdvertising()
{
    advertisingWanted_ = false;
    NimBLEDevice::getAdvertising()->stop();
}

void AcaiaPeripheral::disconnectClient()
{
    const uint16_t handle = connHandle_.load();
    if (handle != kNoConnection) {
        server_->disconnect(handle);
    }
}

void AcaiaPeripheral::sendWeight(int32_t weightDg)
{
    notify(encodeWeight(weightDg));
}

void AcaiaPeripheral::onConnect(NimBLEServer* server, NimBLEConnInfo& connInfo)
{
    (void)server;
    ESP_LOGI(TAG, "client connected: %s", connInfo.getAddress().toString().c_str());
    connHandle_.store(connInfo.getConnHandle());
}

void AcaiaPeripheral::onDisconnect(NimBLEServer* server, NimBLEConnInfo& connInfo, int reason)
{
    (void)server;
    ESP_LOGI(TAG, "client disconnected: %s, reason 0x%x", connInfo.getAddress().toString().c_str(), reason);
    connHandle_.store(kNoConnection);
    subscribed_.store(false);
    if (advertisingWanted_) {
        startAdvertising();
    }
}

void AcaiaPeripheral::onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo)
{
    (void)connInfo;
    const NimBLEAttValue value = characteristic->getValue();
    const std::span<const uint8_t> data{value.data(), value.size()};
#if CONFIG_BRIDGE_LOG_ACAIA_FRAMES
    ESP_LOGI(TAG, "rx %u bytes", static_cast<unsigned>(data.size()));
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, data.data(), data.size(), ESP_LOG_INFO);
#endif
    for (const ParsedFrame& frame : split(data)) {
        const Command command = classify(frame);
        if (command == Command::Unknown) {
            ESP_LOGW(TAG, "unhandled message type 0x%02x", frame.type);
        }
        handle(command);
    }
}

void AcaiaPeripheral::onRead(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo)
{
    ESP_LOGI(TAG, "%s read %s", connInfo.getAddress().toString().c_str(),
             characteristic->getUUID().toString().c_str());
}

void AcaiaPeripheral::onSubscribe(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo, uint16_t subValue)
{
    (void)connInfo;
    if (characteristic == notifyChar_) {
        ESP_LOGI(TAG, "notifications %s", subValue != 0 ? "enabled" : "disabled");
        subscribed_.store(subValue != 0);
    }
}

void AcaiaPeripheral::handle(Command command)
{
    ESP_LOGI(TAG, "command: %s", toString(command));
    switch (command) {
    case Command::Ident:
    case Command::NotificationRequest:
    case Command::GetSettings:
        notify(encodeStatus(batteryPct_.load()));
        break;
    case Command::Tare:
    case Command::TimerStart:
    case Command::TimerStop:
    case Command::TimerReset:
        if (sink_ != nullptr) {
            sink_->onAcaiaCommand(command);
        }
        break;
    case Command::Heartbeat:
    case Command::Unknown:
        break;
    }
}

void AcaiaPeripheral::notify(const Frame& frame)
{
    if (!subscribed_.load() || !isClientConnected()) {
        return;
    }
#if CONFIG_BRIDGE_LOG_ACAIA_FRAMES
    if (frame.size() > 3 && frame[2] != static_cast<uint8_t>(MessageType::Event)) {
        ESP_LOGI(TAG, "tx %u bytes", static_cast<unsigned>(frame.size()));
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, frame.data(), frame.size(), ESP_LOG_INFO);
    }
#endif
    notifyChar_->notify(frame.data(), frame.size());
}

} // namespace acaia
