#pragma once

/*
 * DIY scales from beeb/coffee-scale-app, both read-only (tare on the device).
 *
 * Sources: Beanconqueror diyPythonCoffeeScale.ts / diyRustCoffeeScale.ts and
 * the firmware itself. Confidence: two sources plus firmware.
 *
 * Python ("mpy-coffee", unmaintained): service 1815, notify 2A59, int16 BE, 0.01 g.
 * Rust ("coffee-scale"): service 181D, notify 2A9D, int32 BE, 0.01 g. Despite the
 * UUID this is not the Bluetooth SIG weight-measurement format.
 */

#include <optional>

#include "scale/Driver.hpp"

namespace scale {

class DiyPythonScale : public DecodingDriver<DiyPythonScale> {
public:
    static constexpr const char* kName = "DIY mpy-coffee";
    static bool matches(const Advertisement& advertisement);

    static std::optional<Reading> decode(std::span<const uint8_t> data);

    const GattProfile& profile() const override;
    bool send(Command, Transport&) override { return false; }
};

class DiyRustScale : public DecodingDriver<DiyRustScale> {
public:
    static constexpr const char* kName = "DIY coffee-scale";
    static bool matches(const Advertisement& advertisement);

    static std::optional<Reading> decode(std::span<const uint8_t> data);

    const GattProfile& profile() const override;
    bool send(Command, Transport&) override { return false; }
};

} // namespace scale
