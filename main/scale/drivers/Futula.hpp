#pragma once

/*
 * Futula Kitchen Scale 3 / LEFU CK811 ("LFSmart Scale").
 *
 * Sources: Beanconqueror futulaScale.ts, wdrs/FutulaCoffeeScale,
 * DrSkyFly/Pourista (tested on hardware there). Confidence: three sources.
 *
 * Service FFF0, notify FFF4, write FFF1 (with response).
 * Weight: u16 LE at [3..4], 0.1 g; negative if [5] > 0; [8] = display unit,
 * the value is grams regardless. Commands: 11 bytes, FD <cmd> <arg> 00.. <xor of 0..9>.
 * No timer. Beanconqueror also matches a bare "lefu", which Lefu body scales share.
 */

#include <array>
#include <optional>

#include "scale/Driver.hpp"

namespace scale {

class Futula : public DecodingDriver<Futula> {
public:
    static constexpr const char* kName = "Futula";
    static bool matches(const Advertisement& advertisement);

    using Frame = std::array<uint8_t, 11>;
    static std::optional<Reading> decode(std::span<const uint8_t> data);
    static Frame frame(uint8_t command, uint8_t argument);

    const GattProfile& profile() const override;
    bool send(Command command, Transport& transport) override;

protected:
    void onConnected(Transport& transport) override;
};

} // namespace scale
