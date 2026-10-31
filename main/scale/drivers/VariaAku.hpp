#pragma once

/*
 * Varia AKU / AKU Mini (de1app says Pro, Plus and Micro too; their advertised
 * names are unknown).
 *
 * Sources: Beanconqueror variaAku.ts, Decent de1app. Confidence: two sources
 * for tare and weight; timer commands Beanconqueror only.
 *
 * Service FFF0, notify FFF1, write FFF2 (without response).
 * Frames: FA <cmd> <len> <payload> <xor>. Weight: cmd 01 len 03; payload[0]
 * bit 0x10 = negative, low nibble + payload[1..2] = 20-bit value in 0.01 g.
 * Commands: FA <cmd> 01 01 <xor of cmd 01 01>.
 */

#include <array>
#include <optional>

#include "scale/Driver.hpp"

namespace scale {

class VariaAku : public DecodingDriver<VariaAku> {
public:
    static constexpr const char* kName = "Varia AKU";
    static bool matches(const Advertisement& advertisement);

    using Frame = std::array<uint8_t, 5>;
    static std::optional<Reading> decode(std::span<const uint8_t> data);
    static std::optional<Frame> encode(Command command);

    const GattProfile& profile() const override;
    bool send(Command command, Transport& transport) override;
};

} // namespace scale
