#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace ota {

/**
 * Start of an ESP-IDF app image: image header (24 bytes), first segment header
 * (8 bytes), then esp_app_desc_t (256 bytes). Parsed without ESP-IDF headers so
 * the check runs on the host; the layout is fixed by the image format.
 */
struct AppImage {
    static constexpr size_t kHeaderLength = 24 + 8 + 256;
    static constexpr uint16_t kChipIdEsp32s3 = 9;

    uint16_t chipId;
    std::string projectName;
    std::string version;

    /** nullopt unless `data` holds at least kHeaderLength bytes of a valid app image start. */
    static std::optional<AppImage> parse(std::span<const uint8_t> data);
};

} // namespace ota
