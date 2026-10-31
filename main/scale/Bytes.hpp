#pragma once

/** Byte-order and checksum helpers shared by the scale drivers. */

#include <cstdint>
#include <cstring>
#include <span>

namespace scale::bytes {

inline uint16_t be16(std::span<const uint8_t> d, std::size_t i)
{
    return static_cast<uint16_t>(d[i] << 8 | d[i + 1]);
}

inline uint16_t le16(std::span<const uint8_t> d, std::size_t i)
{
    return static_cast<uint16_t>(d[i] | d[i + 1] << 8);
}

inline uint32_t be24(std::span<const uint8_t> d, std::size_t i)
{
    return static_cast<uint32_t>(d[i]) << 16 | static_cast<uint32_t>(d[i + 1]) << 8 | d[i + 2];
}

inline uint32_t le24(std::span<const uint8_t> d, std::size_t i)
{
    return d[i] | static_cast<uint32_t>(d[i + 1]) << 8 | static_cast<uint32_t>(d[i + 2]) << 16;
}

inline uint32_t be32(std::span<const uint8_t> d, std::size_t i)
{
    return static_cast<uint32_t>(d[i]) << 24 | be24(d, i + 1);
}

inline uint32_t le32(std::span<const uint8_t> d, std::size_t i)
{
    return le24(d, i) | static_cast<uint32_t>(d[i + 3]) << 24;
}

inline float leFloat(std::span<const uint8_t> d, std::size_t i)
{
    const uint32_t raw = le32(d, i);
    float value;
    std::memcpy(&value, &raw, sizeof(value));
    return value;
}

inline uint8_t xorOf(std::span<const uint8_t> d)
{
    uint8_t x = 0;
    for (uint8_t b : d) {
        x ^= b;
    }
    return x;
}

inline uint8_t sumOf(std::span<const uint8_t> d)
{
    uint8_t s = 0;
    for (uint8_t b : d) {
        s = static_cast<uint8_t>(s + b);
    }
    return s;
}

} // namespace scale::bytes
