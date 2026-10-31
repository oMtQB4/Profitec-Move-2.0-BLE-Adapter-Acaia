#pragma once

/*
 * Smartchef.
 *
 * Sources: Beanconqueror smartchefScale.ts, Decent de1app (identical decoding).
 * Confidence: two sources.
 *
 * Service FFF0, notify FFF1; no commands (tare only on the scale; the bridge
 * zeroes in software). Weight: u16 BE at [5..6], 0.1 g; negative if [3] > 10.
 */

#include <optional>

#include "scale/Driver.hpp"

namespace scale {

class Smartchef : public DecodingDriver<Smartchef> {
public:
    static constexpr const char* kName = "Smartchef";
    static bool matches(const Advertisement& advertisement);

    static std::optional<Reading> decode(std::span<const uint8_t> data);

    const GattProfile& profile() const override;
    bool send(Command, Transport&) override { return false; }
};

} // namespace scale
