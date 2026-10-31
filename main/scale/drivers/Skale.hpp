#pragma once

/*
 * Atomax Skale / Skale II.
 *
 * Sources: Decent de1app (weight decoding, timer bytes), Beanconqueror skale.ts
 * (whose weight decoding is buggy for negative and larger values).
 * Confidence: two sources; timer bytes de1app only.
 *
 * Service FF08, write EF80, weight notify EF81 (int16 LE at [1..2], 0.1 g).
 * Single-byte commands; tare sent twice.
 */

#include <optional>

#include "scale/Driver.hpp"

namespace scale {

class Skale : public DecodingDriver<Skale> {
public:
    static constexpr const char* kName = "Skale";
    static bool matches(const Advertisement& advertisement);

    static std::optional<Reading> decode(std::span<const uint8_t> data);
    static std::optional<uint8_t> encode(Command command);

    const GattProfile& profile() const override;
    bool send(Command command, Transport& transport) override;

protected:
    void onConnected(Transport& transport) override;
};

} // namespace scale
