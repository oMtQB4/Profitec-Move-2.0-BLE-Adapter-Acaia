#include "config/ConfigService.hpp"

#include <algorithm>
#include <cstring>

#include <esp_log.h>

#include "config/SettingsStore.hpp"

namespace config {

namespace {

constexpr const char* TAG = "config";
constexpr UBaseType_t kQueueLength = 4;
constexpr uint16_t kMaxValueLength = 128;

const char* toString(AutoTareMode mode)
{
    return mode == AutoTareMode::CupPlaced ? "cup placed" : "machine connects";
}

const char* toString(status::LedMode mode)
{
    switch (mode) {
    case status::LedMode::Standard:
        return "standard";
    case status::LedMode::ActiveOnly:
        return "only when active";
    case status::LedMode::Off:
        return "off";
    }
    return "?";
}

void logSettings(const char* what, const Settings& s)
{
    ESP_LOGI(TAG, "%s: auto-tare %s (%s), tare scale %s, LED %s at %u", what, s.autoTare ? "on" : "off",
             toString(s.autoTareMode), s.tareScale ? "on" : "off", toString(s.ledMode),
             static_cast<unsigned>(s.ledBrightness));
}

} // namespace

ConfigService::ConfigService(const Settings& settings)
    : settings_(settings)
    , queue_(xQueueCreate(kQueueLength, sizeof(Message)))
{
    logSettings("settings", settings_);
}

ConfigService::~ConfigService()
{
    vQueueDelete(queue_);
}

void ConfigService::registerService(NimBLEServer& server)
{
    NimBLEService* service = server.createService(kServiceUuid);
    settingsChar_ = service->createCharacteristic(
        kSettingsUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY, kMaxValueLength);
    statusChar_ = service->createCharacteristic(kStatusUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY,
                                                kMaxValueLength);
    controlChar_ = service->createCharacteristic(kControlUuid, NIMBLE_PROPERTY::WRITE, 16);
    settingsChar_->setCallbacks(this);
    controlChar_->setCallbacks(this);
    settingsChar_->setValue(encodeSettings(settings_));
}

void ConfigService::onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo)
{
    (void)connInfo;
    const NimBLEAttValue value = characteristic->getValue();
    Message message{};
    message.kind = characteristic == controlChar_ ? Message::Kind::Control : Message::Kind::Settings;
    message.length = static_cast<uint8_t>(std::min<size_t>(value.size(), sizeof(message.data)));
    std::memcpy(message.data, value.data(), message.length);
    if (xQueueSend(queue_, &message, 0) != pdTRUE) {
        ESP_LOGW(TAG, "write queue full, dropping a write");
    }
}

ConfigService::Events ConfigService::poll()
{
    Events events;
    Message message;
    while (xQueueReceive(queue_, &message, 0) == pdTRUE) {
        const std::span<const uint8_t> data{message.data, message.length};
        if (message.kind == Message::Kind::Control) {
            if (!data.empty() && data[0] == static_cast<uint8_t>(ControlOp::ForgetMachine)) {
                ESP_LOGI(TAG, "forget the machine");
                events.forgetMachine = true;
            } else {
                ESP_LOGW(TAG, "unknown control opcode");
            }
            continue;
        }

        Settings updated = settings_;
        if (!decodeSettings(data, updated)) {
            ESP_LOGW(TAG, "invalid settings write ignored");
        } else if (updated != settings_) {
            settings_ = updated;
            SettingsStore::save(settings_);
            logSettings("settings changed", settings_);
            events.settings = settings_;
        }
        publishSettings(); // the writer reads back what applies, also after an invalid write
    }
    return events;
}

void ConfigService::publishSettings()
{
    const std::vector<uint8_t> value = encodeSettings(settings_);
    settingsChar_->setValue(value);
    settingsChar_->notify();
}

void ConfigService::setStatus(const Status& status)
{
    if (status_ == status) {
        return;
    }
    status_ = status;
    statusChar_->setValue(encodeStatus(status));
    statusChar_->notify();
}

} // namespace config
