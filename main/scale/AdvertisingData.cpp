#include "scale/AdvertisingData.hpp"

#include <algorithm>

namespace scale {

namespace {

constexpr uint8_t kIncompleteServices16 = 0x02;
constexpr uint8_t kCompleteServices16 = 0x03;
constexpr uint8_t kIncompleteServices128 = 0x06;
constexpr uint8_t kCompleteServices128 = 0x07;
constexpr uint8_t kShortenedLocalName = 0x08;
constexpr uint8_t kCompleteLocalName = 0x09;

/**
 * Calls `visit(type, value)` for each AD structure; stops when it returns true.
 * Skips zero padding instead of stopping at it (see header).
 */
template <class Visitor>
void forEachStructure(std::span<const uint8_t> payload, Visitor visit)
{
    std::size_t i = 0;
    while (i < payload.size()) {
        const std::size_t length = payload[i];
        if (length == 0) {
            i++; // padding
            continue;
        }
        if (i + length >= payload.size()) {
            return; // truncated structure
        }
        if (visit(payload[i + 1], payload.subspan(i + 2, length - 1))) {
            return;
        }
        i += length + 1;
    }
}

} // namespace

std::string_view findLocalName(std::span<const uint8_t> payload)
{
    std::string_view complete;
    std::string_view shortened;
    forEachStructure(payload, [&](uint8_t type, std::span<const uint8_t> value) {
        const std::string_view text{reinterpret_cast<const char*>(value.data()), value.size()};
        if (type == kCompleteLocalName) {
            complete = text;
            return true;
        }
        if (type == kShortenedLocalName && shortened.empty()) {
            shortened = text;
        }
        return false;
    });
    return complete.empty() ? shortened : complete;
}

bool advertisesService16(std::span<const uint8_t> payload, uint16_t uuid)
{
    bool found = false;
    forEachStructure(payload, [&](uint8_t type, std::span<const uint8_t> value) {
        if (type != kIncompleteServices16 && type != kCompleteServices16) {
            return false;
        }
        for (std::size_t i = 0; i + 1 < value.size(); i += 2) {
            if ((value[i] | value[i + 1] << 8) == uuid) {
                found = true;
            }
        }
        return found;
    });
    return found;
}

bool advertisesService128(std::span<const uint8_t> payload, const Uuid128& uuid)
{
    bool found = false;
    forEachStructure(payload, [&](uint8_t type, std::span<const uint8_t> value) {
        if (type != kIncompleteServices128 && type != kCompleteServices128) {
            return false;
        }
        for (std::size_t i = 0; i + 16 <= value.size(); i += 16) {
            if (std::equal(uuid.begin(), uuid.end(), value.begin() + static_cast<std::ptrdiff_t>(i))) {
                found = true;
            }
        }
        return found;
    });
    return found;
}

} // namespace scale
