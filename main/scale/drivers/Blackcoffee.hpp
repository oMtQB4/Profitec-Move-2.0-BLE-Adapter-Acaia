#pragma once

/*
 * Blackcoffee.io scale.
 *
 * Source: Beanconqueror blackcoffeeScale.ts only (sign encoding undocumented
 * elsewhere). Confidence: single source.
 *
 * Service FFB0, notify FFB2; no commands. Frames >= 15 bytes: high nibble of
 * [2] is 8 or C for negative; magnitude = low nibble of [3] + [4..6], 0.001 g.
 */

#include <optional>

#include "scale/Driver.hpp"

namespace scale {

class Blackcoffee : public DecodingDriver<Blackcoffee> {
public:
    static constexpr const char* kName = "Blackcoffee";
    static bool matches(const Advertisement& advertisement);

    static std::optional<Reading> decode(std::span<const uint8_t> data);

    const GattProfile& profile() const override;
    bool send(Command, Transport&) override { return false; }
};

} // namespace scale
