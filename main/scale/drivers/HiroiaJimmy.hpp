#pragma once

/*
 * Hiroia Jimmy.
 *
 * Sources: Beanconqueror jimmyScale.ts, Decent de1app. Confidence: two sources
 * for UUIDs, tare and weight layout; ounce mode from Beanconqueror only.
 *
 * Service 06C31822-..., write ...23, notify ...24.
 * Weight: [0] mode (>8 = ounces), [4..6] int24 LE (grams x10, ounces x1000).
 * Tare 07 00 (sent twice). The timer is toggle-only, so it is not driven.
 */

#include <optional>

#include "scale/Driver.hpp"

namespace scale {

class HiroiaJimmy : public DecodingDriver<HiroiaJimmy> {
public:
    static constexpr const char* kName = "Hiroia Jimmy";
    static bool matches(const Advertisement& advertisement);

    static std::optional<Reading> decode(std::span<const uint8_t> data);

    const GattProfile& profile() const override;
    bool send(Command command, Transport& transport) override;
};

} // namespace scale
