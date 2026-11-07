#include "config/ConfigProtocol.hpp"

#include <algorithm>
#include <utility>

namespace config {

namespace {

class Writer {
public:
    void u8(uint8_t key, uint8_t value) { record(key, {&value, 1}); }

    void text(uint8_t key, const std::string& value)
    {
        const size_t length = std::min<size_t>(value.size(), 255);
        record(key, {reinterpret_cast<const uint8_t*>(value.data()), length});
    }

    void record(uint8_t key, std::span<const uint8_t> value)
    {
        out_.push_back(key);
        out_.push_back(static_cast<uint8_t>(value.size()));
        out_.insert(out_.end(), value.begin(), value.end());
    }

    std::vector<uint8_t> take() { return std::move(out_); }

private:
    std::vector<uint8_t> out_;
};

template <typename T>
uint8_t raw(T key)
{
    return static_cast<uint8_t>(key);
}

bool decodeBool(uint8_t value, bool& out)
{
    if (value > 1) {
        return false;
    }
    out = value != 0;
    return true;
}

} // namespace

std::vector<uint8_t> encodeSettings(const Settings& settings)
{
    Writer writer;
    writer.u8(raw(SettingKey::Version), kProtocolVersion);
    writer.u8(raw(SettingKey::AutoTare), settings.autoTare ? 1 : 0);
    writer.u8(raw(SettingKey::AutoTareMode), raw(settings.autoTareMode));
    writer.u8(raw(SettingKey::TareScale), settings.tareScale ? 1 : 0);
    writer.u8(raw(SettingKey::LedMode), raw(settings.ledMode));
    writer.u8(raw(SettingKey::LedBrightness), settings.ledBrightness);
    return writer.take();
}

bool decodeSettings(std::span<const uint8_t> data, Settings& settings)
{
    Settings result = settings;
    size_t offset = 0;
    while (offset < data.size()) {
        if (data.size() - offset < 2 || data.size() - offset - 2 < data[offset + 1]) {
            return false;
        }
        const auto key = static_cast<SettingKey>(data[offset]);
        const std::span<const uint8_t> value = data.subspan(offset + 2, data[offset + 1]);
        offset += 2 + value.size();

        const bool known = key == SettingKey::AutoTare || key == SettingKey::AutoTareMode ||
                           key == SettingKey::TareScale || key == SettingKey::LedMode ||
                           key == SettingKey::LedBrightness;
        if (!known) {
            continue; // Version or a key from a newer configurator
        }
        if (value.size() != 1) {
            return false;
        }
        const uint8_t v = value[0];
        switch (key) {
        case SettingKey::AutoTare:
            if (!decodeBool(v, result.autoTare)) {
                return false;
            }
            break;
        case SettingKey::AutoTareMode:
            if (v > raw(AutoTareMode::MachineConnects)) {
                return false;
            }
            result.autoTareMode = static_cast<AutoTareMode>(v);
            break;
        case SettingKey::TareScale:
            if (!decodeBool(v, result.tareScale)) {
                return false;
            }
            break;
        case SettingKey::LedMode:
            if (v > raw(status::LedMode::Off)) {
                return false;
            }
            result.ledMode = static_cast<status::LedMode>(v);
            break;
        case SettingKey::LedBrightness:
            if (v == 0) {
                return false;
            }
            result.ledBrightness = v;
            break;
        case SettingKey::Version:
            break;
        }
    }
    settings = result;
    return true;
}

std::vector<uint8_t> encodeStatus(const Status& status)
{
    Writer writer;
    writer.u8(raw(StatusKey::Version), kProtocolVersion);
    writer.u8(raw(StatusKey::Mode), raw(status.mode));
    writer.text(raw(StatusKey::Firmware), status.firmware);
    writer.u8(raw(StatusKey::ScaleConnected), status.scaleConnected ? 1 : 0);
    if (status.scaleConnected && !status.scaleName.empty()) {
        writer.text(raw(StatusKey::ScaleName), status.scaleName);
    }
    writer.u8(raw(StatusKey::Client), raw(status.client));
    if (status.machine) {
        writer.text(raw(StatusKey::Machine), *status.machine);
    }
    return writer.take();
}

} // namespace config
