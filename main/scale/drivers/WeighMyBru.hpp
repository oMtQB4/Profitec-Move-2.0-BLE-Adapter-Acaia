#pragma once

/*
 * WeighMyBru (open-source ESP32 scale).
 *
 * Sources: Beanconqueror weighMyBruScale.ts, the weighmybru2 firmware,
 * GaggiMate's scales library. Confidence: two sources plus firmware.
 *
 * Nordic-UART-style service 6E400001-..., weight notify 6E400004 (float32 LE,
 * grams, every 50 ms), write 6E400003. Commands 03 0A <action> 01 00;
 * the firmware checks no checksum.
 */

#include <array>
#include <optional>

#include "scale/Driver.hpp"

namespace scale {

class WeighMyBru : public DecodingDriver<WeighMyBru> {
public:
    static constexpr const char* kName = "WeighMyBru";
    static bool matches(const Advertisement& advertisement);

    using Frame = std::array<uint8_t, 5>;
    static std::optional<Reading> decode(std::span<const uint8_t> data);
    static std::optional<Frame> encode(Command command);

    const GattProfile& profile() const override;
    bool send(Command command, Transport& transport) override;
};

} // namespace scale
