#pragma once

/*
 * Decent Scale (also "ButtsHaus Scale" clones) and EspressiScale (DIY clone of
 * the same protocol).
 *
 * Sources: Beanconqueror decentScale.ts / espressiScale.ts, Decent de1app
 * bluetooth.tcl + binary.tcl, Whauu/EspressiScale firmware. Confidence: two
 * or more sources.
 *
 * Service FFF0, notify FFF4, write 36F5. Frames: 03 <cmd> d1..d4 <xor of 0..5>.
 * Weight: 03 CE|CA <int16 BE, 0.1 g> ..., 7 bytes (API < 1.3) or 10 bytes.
 * v1.0 firmware occasionally drops commands, so commands are sent twice.
 */

#include <array>
#include <optional>

#include "scale/Driver.hpp"

namespace scale {

class DecentScale : public DecodingDriver<DecentScale> {
public:
    static constexpr const char* kName = "Decent Scale";
    static bool matches(const Advertisement& advertisement);

    using Frame = std::array<uint8_t, 7>;
    static std::optional<Reading> decode(std::span<const uint8_t> data);
    static Frame frame(uint8_t command, uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4);
    static std::optional<Frame> encode(Command command, uint8_t tareCounter);

    const GattProfile& profile() const override;
    bool send(Command command, Transport& transport) override;

protected:
    void onConnected(Transport& transport) override;
    void onTick(Transport& transport, uint32_t nowMs) override;

private:
    uint8_t tareCounter_ = 253; // de1app starts here; any recently unused value works
    std::optional<uint32_t> lastHeartbeatMs_;
};

} // namespace scale
