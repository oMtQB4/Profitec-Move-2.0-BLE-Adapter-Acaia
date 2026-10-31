#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace scale {

/**
 * Finds the local name in raw advertising data (advertisement followed by scan
 * response). Returns the complete name, else the shortened one, else empty.
 *
 * Unlike esp-nimble-cpp's parser, this skips zero padding instead of stopping
 * at it. The Eureka Precisa pads its advertisement to 31 bytes with zeros and
 * sends its name in the scan response behind that padding.
 */
std::string_view findLocalName(std::span<const uint8_t> payload);

} // namespace scale
