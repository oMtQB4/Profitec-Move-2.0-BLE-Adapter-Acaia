#pragma once

/*
 * Felicita Arc / Parallel / Incline (whether all models share the protocol is
 * unconfirmed).
 *
 * Sources: Beanconqueror felicitaScale.ts (+ readme with a captured frame),
 * Decent de1app. Confidence: two sources.
 *
 * Service FFE0, one characteristic FFE1 for notify and write.
 * Weight (18 bytes): 01 02 <'+'|'-'> <6 ASCII digits, 0.01 g> <" g"> ... 0D 0A.
 * Commands are single ASCII bytes.
 */

#include <optional>

#include "scale/Driver.hpp"

namespace scale {

class Felicita : public DecodingDriver<Felicita> {
public:
    static constexpr const char* kName = "Felicita";
    static bool matches(const Advertisement& advertisement);

    static std::optional<Reading> decode(std::span<const uint8_t> data);
    static std::optional<uint8_t> encode(Command command);

    const GattProfile& profile() const override;
    bool send(Command command, Transport& transport) override;
};

} // namespace scale
