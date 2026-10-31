#pragma once

/*
 * Bookoo Themis (Mini, Ultra).
 *
 * Sources: Bookoo's official protocol docs (BooKooCode/OpenSource), Beanconqueror
 * bokooScale.ts, Decent de1app, AcaiaArduinoBLE. Confidence: four sources.
 *
 * Service 0FFE, notify FF11, write FF12.
 * Weight frame (20 bytes): 03 0B <ms u24> <unit> <sign '+'|'-'> <u24 BE, 0.01 g> ...
 * The Ultra also sends 20-byte 03 0F / 03 0D frames, so [1] must be checked.
 * Commands: 03 0A d1 d2 d3 cs; the literal bytes all implementations use are
 * kept even where cs is not the documented XOR (the scale accepts them).
 */

#include <array>
#include <optional>

#include "scale/Driver.hpp"

namespace scale {

class BookooThemis : public DecodingDriver<BookooThemis> {
public:
    static constexpr const char* kName = "Bookoo Themis";
    static bool matches(const Advertisement& advertisement);

    using Frame = std::array<uint8_t, 6>;
    static std::optional<Reading> decode(std::span<const uint8_t> data);
    static std::optional<Frame> encode(Command command);

    const GattProfile& profile() const override;
    bool send(Command command, Transport& transport) override;
};

} // namespace scale
