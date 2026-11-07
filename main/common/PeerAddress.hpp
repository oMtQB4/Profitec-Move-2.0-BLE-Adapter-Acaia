#pragma once

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>

/** A Bluetooth device address as NimBLE stores it: value[0] is the least significant byte. */
struct PeerAddress {
    static constexpr uint8_t kPublic = 0;
    static constexpr uint8_t kRandom = 1;

    std::array<uint8_t, 6> value{};
    uint8_t type = kPublic;

    bool operator==(const PeerAddress&) const = default;

    /**
     * Public and static random addresses stay the same across connections.
     * Resolvable private addresses (phones, most computers) change every few
     * minutes and are useless for recognising a device again.
     */
    bool isStable() const { return type == kPublic || (type == kRandom && (value[5] & 0xC0) == 0xC0); }

    /** "AA:BB:CC:DD:EE:FF", most significant byte first. */
    std::string toString() const
    {
        char text[18];
        std::snprintf(text, sizeof(text), "%02X:%02X:%02X:%02X:%02X:%02X", value[5], value[4], value[3], value[2],
                      value[1], value[0]);
        return text;
    }
};
