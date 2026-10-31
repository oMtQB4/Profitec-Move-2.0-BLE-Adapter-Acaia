#pragma once

/*
 * Mantabrew WeighMaster 2.0 (ECS-02).
 *
 * Source: Beanconqueror weighMasterScale.ts (documented examples); talla's
 * driver has identical bytes and is probably derived. Confidence: single source.
 *
 * Service FFF0, notify FFF4, write FFF1 (without response).
 * Weight (7 bytes): 01 02 00 <status> <u24 BE, 0.1 g>; status bit 0x10 = negative.
 * Tare 02, then a short beep 05 00 after 300 ms. Timer 52 0B <01 start|00 stop|02 reset> 00 00 00 00.
 */

#include <array>
#include <optional>

#include "scale/Driver.hpp"

namespace scale {

class WeighMaster : public DecodingDriver<WeighMaster> {
public:
    static constexpr const char* kName = "WeighMaster";
    static bool matches(const Advertisement& advertisement);

    using TimerFrame = std::array<uint8_t, 7>;
    static std::optional<Reading> decode(std::span<const uint8_t> data);
    static TimerFrame timerFrame(uint8_t action);

    const GattProfile& profile() const override;
    bool send(Command command, Transport& transport) override;
};

} // namespace scale
