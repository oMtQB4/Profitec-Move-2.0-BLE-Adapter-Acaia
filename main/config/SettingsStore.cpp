#include "config/SettingsStore.hpp"

#include <algorithm>
#include <vector>

#include <esp_log.h>
#include <nvs.h>

#include "config/ConfigProtocol.hpp"

namespace config {

namespace {

constexpr const char* TAG = "config";
constexpr const char* kNamespace = "config";
constexpr const char* kSettingsKey = "settings";
constexpr const char* kMachineKey = "machine";

class Handle {
public:
    explicit Handle(nvs_open_mode_t mode) { err_ = nvs_open(kNamespace, mode, &handle_); }
    ~Handle()
    {
        if (err_ == ESP_OK) {
            nvs_close(handle_);
        }
    }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    bool ok() const { return err_ == ESP_OK; }
    esp_err_t error() const { return err_; }
    nvs_handle_t get() const { return handle_; }

private:
    nvs_handle_t handle_ = 0;
    esp_err_t err_;
};

std::optional<std::vector<uint8_t>> readBlob(const Handle& handle, const char* key)
{
    size_t size = 0;
    if (nvs_get_blob(handle.get(), key, nullptr, &size) != ESP_OK) {
        return std::nullopt;
    }
    std::vector<uint8_t> data(size);
    if (nvs_get_blob(handle.get(), key, data.data(), &size) != ESP_OK) {
        return std::nullopt;
    }
    return data;
}

bool commit(const Handle& handle, esp_err_t err, const char* what)
{
    if (err == ESP_OK) {
        err = nvs_commit(handle.get());
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "saving %s failed: %s", what, esp_err_to_name(err));
    }
    return err == ESP_OK;
}

} // namespace

Settings SettingsStore::load(const Settings& defaults)
{
    Settings settings = defaults;
    const Handle handle(NVS_READONLY);
    if (!handle.ok()) {
        return settings; // nothing saved yet
    }
    const auto data = readBlob(handle, kSettingsKey);
    if (data && !decodeSettings(*data, settings)) {
        ESP_LOGW(TAG, "stored settings invalid, using defaults");
        settings = defaults;
    }
    return settings;
}

bool SettingsStore::save(const Settings& settings)
{
    const Handle handle(NVS_READWRITE);
    if (!handle.ok()) {
        ESP_LOGW(TAG, "nvs_open: %s", esp_err_to_name(handle.error()));
        return false;
    }
    const std::vector<uint8_t> data = encodeSettings(settings);
    return commit(handle, nvs_set_blob(handle.get(), kSettingsKey, data.data(), data.size()), "settings");
}

std::optional<PeerAddress> SettingsStore::loadMachine()
{
    const Handle handle(NVS_READONLY);
    if (!handle.ok()) {
        return std::nullopt;
    }
    const auto data = readBlob(handle, kMachineKey);
    if (!data || data->size() != 7) {
        return std::nullopt;
    }
    PeerAddress address;
    address.type = (*data)[0];
    std::copy(data->begin() + 1, data->end(), address.value.begin());
    return address;
}

bool SettingsStore::saveMachine(const std::optional<PeerAddress>& address)
{
    const Handle handle(NVS_READWRITE);
    if (!handle.ok()) {
        ESP_LOGW(TAG, "nvs_open: %s", esp_err_to_name(handle.error()));
        return false;
    }
    if (!address) {
        const esp_err_t err = nvs_erase_key(handle.get(), kMachineKey);
        return commit(handle, err == ESP_ERR_NVS_NOT_FOUND ? ESP_OK : err, "machine");
    }
    uint8_t data[7];
    data[0] = address->type;
    std::copy(address->value.begin(), address->value.end(), data + 1);
    return commit(handle, nvs_set_blob(handle.get(), kMachineKey, data, sizeof(data)), "machine");
}

} // namespace config
