#include "ota/AppImage.hpp"

#include <algorithm>

namespace ota {

namespace {

constexpr uint8_t kImageMagic = 0xE9;
constexpr size_t kChipIdOffset = 12;
constexpr size_t kAppDescOffset = 24 + 8;
constexpr uint32_t kAppDescMagic = 0xABCD5432;
constexpr size_t kVersionOffset = kAppDescOffset + 16;
constexpr size_t kProjectNameOffset = kAppDescOffset + 48;
constexpr size_t kStringLength = 32;

std::string fixedString(std::span<const uint8_t> field)
{
    const auto end = std::find(field.begin(), field.end(), uint8_t{0});
    return std::string(field.begin(), end);
}

} // namespace

std::optional<AppImage> AppImage::parse(std::span<const uint8_t> data)
{
    if (data.size() < kHeaderLength || data[0] != kImageMagic) {
        return std::nullopt;
    }
    const uint32_t descMagic = static_cast<uint32_t>(data[kAppDescOffset]) |
                               static_cast<uint32_t>(data[kAppDescOffset + 1]) << 8 |
                               static_cast<uint32_t>(data[kAppDescOffset + 2]) << 16 |
                               static_cast<uint32_t>(data[kAppDescOffset + 3]) << 24;
    if (descMagic != kAppDescMagic) {
        return std::nullopt;
    }
    return AppImage{
        .chipId = static_cast<uint16_t>(data[kChipIdOffset] | data[kChipIdOffset + 1] << 8),
        .projectName = fixedString(data.subspan(kProjectNameOffset, kStringLength)),
        .version = fixedString(data.subspan(kVersionOffset, kStringLength)),
    };
}

} // namespace ota
