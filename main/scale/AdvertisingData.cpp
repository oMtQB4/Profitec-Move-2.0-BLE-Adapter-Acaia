#include "scale/AdvertisingData.hpp"

namespace scale {

namespace {

constexpr uint8_t kShortenedLocalName = 0x08;
constexpr uint8_t kCompleteLocalName = 0x09;

} // namespace

std::string_view findLocalName(std::span<const uint8_t> payload)
{
    std::string_view shortened;
    std::size_t i = 0;
    while (i < payload.size()) {
        const std::size_t length = payload[i];
        if (length == 0) {
            i++; // padding
            continue;
        }
        if (i + length >= payload.size()) {
            break; // truncated structure
        }
        const uint8_t type = payload[i + 1];
        const std::string_view value{reinterpret_cast<const char*>(&payload[i + 2]), length - 1};
        if (type == kCompleteLocalName) {
            return value;
        }
        if (type == kShortenedLocalName && shortened.empty()) {
            shortened = value;
        }
        i += length + 1;
    }
    return shortened;
}

} // namespace scale
