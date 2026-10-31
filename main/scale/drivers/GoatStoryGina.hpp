#pragma once

/*
 * Goat Story Gina (smart scale built into the Gina brewer).
 *
 * Source: talla-speciality/talla (iOS + Android drivers and a unit test);
 * apparently reverse-engineered. Confidence: single source.
 *
 * Service 91341521-BAC2-42D9-BBB3-942F7A10976C, weight notify ...22, calibration
 * read ...23. Each scale stores its own divisor: factor = u32 LE(calibration) / 10000,
 * grams = u32 LE(weight) / 10 / factor. No commands, no sign.
 * Matched by service UUID only: its advertised name is not documented, and a
 * name match on "gina" would also catch e.g. phones.
 */

#include <optional>

#include "scale/Driver.hpp"

namespace scale {

class GoatStoryGina : public Driver {
public:
    static constexpr const char* kName = "Goat Story Gina";
    static bool matches(const Advertisement& advertisement);

    static std::optional<double> calibrationFactor(std::span<const uint8_t> calibration);
    static std::optional<Reading> decode(std::span<const uint8_t> data, double factor);

    std::string_view name() const override { return kName; }
    const GattProfile& profile() const override;
    void onNotify(std::span<const uint8_t> data) override;
    bool send(Command, Transport&) override { return false; }

protected:
    void onConnected(Transport& transport) override;

private:
    std::optional<double> factor_;
};

} // namespace scale
