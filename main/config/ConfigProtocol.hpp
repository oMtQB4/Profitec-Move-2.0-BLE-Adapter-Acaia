#pragma once

/*
 * Configuration over Bluetooth, protocol version 1. The browser side lives in
 * web/configurator/protocol.js; keep both in sync.
 *
 * GATT service kServiceUuid, part of the GATT table in both boot modes (after
 * the update service, so the handles of all earlier services stay put):
 *
 *   Settings (read, write, notify)  the settings as records, see below. A write
 *                                   may carry any subset; it is applied as a
 *                                   whole or not at all, saved, and the full
 *                                   settings are notified back.
 *   Status   (read, notify)         adapter status as records.
 *   Control  (write)                one opcode byte (ControlOp).
 *
 * Records: <key u8> <length u8> <value: length bytes>, repeated. Readers skip
 * unknown keys, so either side may add keys without breaking the other.
 * Integers are little endian; strings UTF-8 without terminator.
 */

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "bridge/ClientIdentifier.hpp"
#include "config/Settings.hpp"

namespace config {

inline constexpr const char* kServiceUuid = "7e1a0010-8f3c-4b6e-9a51-3c7d2e4f6a10";
inline constexpr const char* kSettingsUuid = "7e1a0011-8f3c-4b6e-9a51-3c7d2e4f6a10";
inline constexpr const char* kStatusUuid = "7e1a0012-8f3c-4b6e-9a51-3c7d2e4f6a10";
inline constexpr const char* kControlUuid = "7e1a0013-8f3c-4b6e-9a51-3c7d2e4f6a10";

inline constexpr uint8_t kProtocolVersion = 1;

enum class SettingKey : uint8_t {
    Version = 0,       ///< u8, read only
    AutoTare = 1,      ///< u8 0/1
    AutoTareMode = 2,  ///< u8 AutoTareMode
    TareScale = 3,     ///< u8 0/1
    LedMode = 4,       ///< u8 status::LedMode
    LedBrightness = 5, ///< u8 1-255
};

enum class StatusKey : uint8_t {
    Version = 0,        ///< u8
    Mode = 1,           ///< u8 AdapterMode
    Firmware = 2,       ///< string
    ScaleConnected = 3, ///< u8 0/1
    ScaleName = 4,      ///< string, driver name; absent without scale
    Client = 5,         ///< u8 bridge::ClientKind of the emulated Lunar's client
    Machine = 6,        ///< string "AA:BB:CC:DD:EE:FF", the learned machine; absent if none
};

enum class AdapterMode : uint8_t {
    Bridge = 0,
    Update = 1,
};

enum class ControlOp : uint8_t {
    ForgetMachine = 0x01, ///< the next client that behaves like the machine is learned again
};

struct Status {
    AdapterMode mode = AdapterMode::Bridge;
    std::string firmware;
    bool scaleConnected = false;
    std::string scaleName;
    bridge::ClientKind client = bridge::ClientKind::None;
    std::optional<std::string> machine;

    bool operator==(const Status&) const = default;
};

std::vector<uint8_t> encodeSettings(const Settings& settings);

/**
 * Applies the records in `data` to `settings`. Returns false, leaving
 * `settings` untouched, for a truncated record or a value out of range.
 */
bool decodeSettings(std::span<const uint8_t> data, Settings& settings);

std::vector<uint8_t> encodeStatus(const Status& status);

} // namespace config
