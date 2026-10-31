#pragma once

/*
 * Eureka Precisa (advertises as CFS-9002); Solo Barista LSJ-001 uses the same
 * protocol.
 *
 * Sources: Beanconqueror (src/classes/devices/eurekaPrecisaScale.ts) and
 * Decent de1app (de1plus/bluetooth.tcl).
 *
 * Service FFF0, notify FFF1, write FFF2 (write without response).
 * Commands: AA 02 cc cc. Unit: AA 03 36 uu (00 = g).
 * Weight notification: AA 09 41, [3] timer running, [4..5] timer,
 * [6] sign (1 = negative), [7..8] weight in 0.1 g (little-endian).
 */

#include <array>
#include <optional>

#include "scale/Driver.hpp"

namespace scale {

class EurekaPrecisa : public DecodingDriver<EurekaPrecisa> {
public:
    static constexpr const char* kName = "Eureka Precisa";
    static bool matches(const Advertisement& advertisement);

    using CommandBytes = std::array<uint8_t, 4>;
    static std::optional<Reading> decode(std::span<const uint8_t> data);
    static std::optional<CommandBytes> encode(Command command);
    static CommandBytes encodeUnitGrams();

    const GattProfile& profile() const override;
    bool send(Command command, Transport& transport) override;

protected:
    void onConnected(Transport& transport) override;
};

} // namespace scale
