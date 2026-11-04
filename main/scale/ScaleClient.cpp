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
constexpr uint32_t kTickPeriodMs = 50;
constexpr UBaseType_t kNotificationQueueLength = 16;
// A scan that failed to start, or stopped without telling us, is retried after this.
constexpr uint32_t kScanRetryMs = 1000;
// Long scans are restarted now and then, in case the controller stops reporting.
constexpr uint32_t kScanRestartMs = 60000;
// Waiting for the disconnect after a failed subscribe: give up and scan again after this.
constexpr uint32_t kConnectingGiveUpMs = kConnectTimeoutMs + 5000;

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
        } else {
            checkScan(nowMs());
        }
        break;
    case State::Connected: {
        Notification notification;
        while (xQueueReceive(notifications_, &notification, 0) == pdTRUE) {
            driver_->onNotify({notification.data, notification.length});
        }
        if (nowMs() - lastTickMs_ >= kTickPeriodMs) {
            lastTickMs_ = nowMs();
            driver_->tick(*this, lastTickMs_);
        }
        break;
    }
    case State::Connecting:
        // Only left waiting for onDisconnect after a failed subscribe; don't wait forever.
        if (nowMs() - stateSinceMs_ >= kConnectingGiveUpMs) {
            ESP_LOGW(TAG, "no disconnect after a failed connection, scanning again");
            client_->disconnect();
            handleDisconnect();
        }
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
    bool withResponse = false;
    switch (driver_->profile().writeMode) {
    case WriteMode::WithResponse:
        withResponse = true;
        break;
    case WriteMode::WithoutResponse:
        withResponse = false;
        break;
    case WriteMode::Auto:
        withResponse = !writeChar_->canWriteNoResponse();
        break;
    }
    return writeChar_->writeValue(data.data(), data.size(), withResponse);
}

std::optional<std::vector<uint8_t>> ScaleClient::read(const char* characteristicUuid)
{
    NimBLERemoteCharacteristic* characteristic =
        service_ != nullptr ? service_->getCharacteristic(characteristicUuid) : nullptr;
    if (characteristic == nullptr || !characteristic->canRead()) {
        return std::nullopt;
    }
    const NimBLEAttValue value = characteristic->readValue();
    return std::vector<uint8_t>(value.begin(), value.end());
}

void ScaleClient::onResult(const NimBLEAdvertisedDevice* device)
{
    if (candidateFound_.load()) {
        return;
    }
    const auto& rawPayload = device->getPayload();
    const std::span<const uint8_t> payload{rawPayload.data(), rawPayload.size()};
    std::string name = device->getName();
    if (name.empty()) {
        name = findLocalName(payload);
    }
    const DriverRegistry::Entry* entry = registry_.find(Advertisement{name, payload});
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
    stateSinceMs_ = nowMs();
    scanEnded_.store(false);
    ESP_LOGI(TAG, "scanning for scales");
    if (!NimBLEDevice::getScan()->start(0, false, true)) {
        // E.g. BLE_HS_EBUSY while a connection attempt is still being cancelled.
        // checkScan() retries; without that the adapter would wait for a reset.
        ESP_LOGW(TAG, "scan did not start, retrying");
    }
}

void ScaleClient::checkScan(uint32_t nowMs)
{
    const uint32_t elapsed = nowMs - stateSinceMs_;
    if (!NimBLEDevice::getScan()->isScanning() && elapsed >= kScanRetryMs) {
        ESP_LOGW(TAG, "scan not running, restarting");
        startScan();
    } else if (elapsed >= kScanRestartMs) {
        startScan(); // restart=true stops and starts the running scan
    }
}

void ScaleClient::connect()
{
    NimBLEDevice::getScan()->stop();
    state_ = State::Connecting;
    stateSinceMs_ = nowMs();
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
    driver_->connected(*this, lastTickMs_);
    if (listener_ != nullptr) {
        listener_->onScaleConnected(driver_->name());
    }
}

bool ScaleClient::subscribe()
{
    const GattProfile& profile = driver_->profile();
    service_ = client_->getService(profile.service);
    if (service_ == nullptr) {
        ESP_LOGW(TAG, "service %s not found", profile.service);
        return false;
    }
    NimBLERemoteCharacteristic* notifyChar = service_->getCharacteristic(profile.notify);
    if (notifyChar == nullptr || !notifyChar->canNotify()) {
        ESP_LOGW(TAG, "notify characteristic %s missing", profile.notify);
        return false;
    }
    if (profile.write != nullptr) {
        writeChar_ = service_->getCharacteristic(profile.write);
        if (writeChar_ == nullptr) {
            ESP_LOGW(TAG, "write characteristic %s missing", profile.write);
            return false;
        }
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
    service_ = nullptr;
    writeChar_ = nullptr;
    xQueueReset(notifications_);
    if (wasConnected && listener_ != nullptr) {
        listener_->onScaleDisconnected();
    }
    startScan();
}

} // namespace scale
