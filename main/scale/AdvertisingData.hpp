#pragma once

/*
 * Parsing of raw advertising data (advertisement followed by scan response).
 *
 * Unlike esp-nimble-cpp's parser, these skip zero padding instead of stopping
 * at it. The Eureka Precisa pads its advertisement to 31 bytes with zeros and
 * sends its name in the scan response behind that padding.
 */

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace scale {

/** 128-bit UUID in over-the-air (little-endian) byte order. */
using Uuid128 = std::array<uint8_t, 16>;

namespace detail {

constexpr uint8_t hexValue(char c)
{
    return static_cast<uint8_t>(c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10);
}

} // namespace detail

/** Parses "91341521-BAC2-42D9-BBB3-942F7A10976C" at compile time. */
consteval Uuid128 uuid128(const char (&text)[37])
{
    Uuid128 uuid{};
    std::size_t out = 16;
    for (std::size_t i = 0; i < 36; i++) {
        if (text[i] == '-') {
            continue;
        }
        uuid[--out] = static_cast<uint8_t>(detail::hexValue(text[i]) << 4 | detail::hexValue(text[i + 1]));
        i++;
    }
    return uuid;
}

/** Complete local name, else the shortened one, else empty. */
std::string_view findLocalName(std::span<const uint8_t> payload);

bool advertisesService16(std::span<const uint8_t> payload, uint16_t uuid);

bool advertisesService128(std::span<const uint8_t> payload, const Uuid128& uuid);

} // namespace scale
