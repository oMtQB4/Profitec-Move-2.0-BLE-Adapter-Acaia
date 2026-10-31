#include "scale/ScaleClient.hpp"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include <esp_log.h>
#include <esp_timer.h>

#include "scale/AdvertisingData.hpp"

namespace scale {

namespace {

constexpr const char* TAG = "scale";
constexpr uint32_t kConnectTimeoutMs = 5000;
constexpr uint32_t kTickPeriodMs = 100;
constexpr UBaseType_t kNotificationQueueLength = 16;

uint32_t nowMs()
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

} // namespace

ScaleClient::ScaleClient(const DriverRegistry& registry)
    : registry_(registry)
    , notifications_(xQueueCreate(kNotificationQueueLength, sizeof(Notification)))
{
}

ScaleClient::~ScaleClient()
{
    vQueueDelete(notifications_);
}

void ScaleClient::begin()
{
    client_ = NimBLEDevice::createClient();
    client_->setClientCallbacks(this, false);
    client_->setConnectTimeout(kConnectTimeoutMs);

    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(this, false);
    scan->setActiveScan(true); // many scales only put their name in the scan response
    // With duplicate filtering, a missed scan response is never retried and a
    // scale whose name is only in the scan response stays nameless. Report
    // every advertisement instead, and don't keep results, so memory stays flat.
    scan->setDuplicateFilter(0);
    scan->setMaxResults(0);
    scan->setInterval(100);
    scan->setWindow(50); // leave airtime for advertising as the Acaia scale
    startScan();
}

void ScaleClient::poll()
{
    if (disconnected_.exchange(false)) {
        handleDisconnect();
    }

    switch (state_) {
    case State::Scanning:
        if (candidateFound_.load()) {
            connect();
        } else if (scanEnded_.exchange(false)) {
            startScan();
        }
        break;
    case State::Connected: {
        Notification notification;
        while (xQueueReceive(notifications_, &notification, 0) == pdTRUE) {
            driver_->onNotify({notification.data, notification.length});
        }
        if (nowMs() - lastTickMs_ >= kTickPeriodMs) {
            lastTickMs_ = nowMs();
            driver_->onTick(*this);
        }
        break;
    }
    case State::Connecting:
        break;
    }
}

bool ScaleClient::send(Command command)
{
    return driver_ != nullptr && driver_->send(command, *this);
}

bool ScaleClient::write(std::span<const uint8_t> data)
{
    if (writeChar_ == nullptr || !client_->isConnected()) {
        return false;
    }
    return writeChar_->writeValue(data.data(), data.size(), driver_->profile().writeWithResponse);
}

void ScaleClient::onResult(const NimBLEAdvertisedDevice* device)
{
    if (candidateFound_.load()) {
        return;
    }
    std::string name = device->getName();
    if (name.empty()) {
        const auto& payload = device->getPayload();
        name = findLocalName({payload.data(), payload.size()});
    }
    const DriverRegistry::Entry* entry = name.empty() ? nullptr : registry_.find(Advertisement{name});
#if CONFIG_BRIDGE_LOG_SCAN_RESULTS
    // Without duplicate filtering every advertisement lands here; log each named device once.
    static std::vector<uint64_t> logged;
    const uint64_t address = device->getAddress();
    if (!name.empty() && logged.size() < 64 && std::find(logged.begin(), logged.end(), address) == logged.end()) {
        logged.push_back(address);
        ESP_LOGI(TAG, "seen %-20s %s rssi %d%s", name.c_str(), device->getAddress().toString().c_str(),
                 device->getRSSI(), entry != nullptr ? " <- match" : "");
    }
#endif
    if (entry == nullptr) {
        return;
    }
    candidateAddress_ = device->getAddress();
    candidateEntry_ = entry;
    candidateFound_.store(true);
}

void ScaleClient::onScanEnd(const NimBLEScanResults& results, int reason)
{
    (void)results;
    ESP_LOGI(TAG, "scan ended, reason %d", reason);
    scanEnded_.store(true);
}

void ScaleClient::onDisconnect(NimBLEClient* client, int reason)
{
    (void)client;
    ESP_LOGI(TAG, "scale disconnected, reason 0x%x", reason);
    disconnected_.store(true);
}

void ScaleClient::startScan()
{
    state_ = State::Scanning;
    scanEnded_.store(false);
    ESP_LOGI(TAG, "scanning for scales");
    NimBLEDevice::getScan()->start(0, false, true);
}

void ScaleClient::connect()
{
    NimBLEDevice::getScan()->stop();
    state_ = State::Connecting;
    candidateFound_.store(false);

    ESP_LOGI(TAG, "connecting to %.*s at %s", static_cast<int>(candidateEntry_->name.size()),
             candidateEntry_->name.data(), candidateAddress_.toString().c_str());
    if (!client_->connect(candidateAddress_)) {
        ESP_LOGW(TAG, "connect failed");
        startScan();
        return;
    }

    driver_ = candidateEntry_->create();
    driver_->setListener(listener_);
    if (!subscribe()) {
        // onDisconnect fires and handleDisconnect() resumes scanning.
        client_->disconnect();
        return;
    }

    state_ = State::Connected;
    lastTickMs_ = nowMs();
    driver_->onConnected(*this);
    if (listener_ != nullptr) {
        listener_->onScaleConnected(driver_->name());
    }
}

bool ScaleClient::subscribe()
{
    const GattProfile& profile = driver_->profile();
    NimBLERemoteService* service = client_->getService(profile.service);
    if (service == nullptr) {
        ESP_LOGW(TAG, "service %s not found", profile.service);
        return false;
    }
    NimBLERemoteCharacteristic* notifyChar = service->getCharacteristic(profile.notify);
    writeChar_ = service->getCharacteristic(profile.write);
    if (notifyChar == nullptr || writeChar_ == nullptr || !notifyChar->canNotify()) {
        ESP_LOGW(TAG, "characteristics %s/%s missing", profile.notify, profile.write);
        return false;
    }

    auto onNotify = [this](NimBLERemoteCharacteristic*, uint8_t* data, size_t length, bool) {
        Notification notification{};
        notification.length = static_cast<uint8_t>(std::min(length, sizeof(notification.data)));
        std::memcpy(notification.data, data, notification.length);
        if (xQueueSend(notifications_, &notification, 0) != pdTRUE) {
            ESP_LOGW(TAG, "notification queue full, dropping");
        }
    };
    if (!notifyChar->subscribe(true, onNotify, true)) {
        ESP_LOGW(TAG, "subscribe failed");
        return false;
    }
    return true;
}

void ScaleClient::handleDisconnect()
{
    const bool wasConnected = state_ == State::Connected;
    driver_.reset();
    writeChar_ = nullptr;
    xQueueReset(notifications_);
    if (wasConnected && listener_ != nullptr) {
        listener_->onScaleDisconnected();
    }
    startScan();
}

} // namespace scale
