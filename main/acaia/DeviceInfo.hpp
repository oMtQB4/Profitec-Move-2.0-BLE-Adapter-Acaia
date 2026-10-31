#pragma once

/*
 * Device Information Service values for the emulated Lunar.
 *
 * A real Lunar 2021 exposes the Device Information Service including a System
 * ID (see aioacaia's services.txt dump). The Make Coffee Yours app cannot see
 * BLE addresses on iOS, so it most likely identifies the scale for the
 * machine's offline mode through these values. Without them the Move stored an
 * all-zero scale identity and never connected.
 */

#include <array>
#include <cstdint>

namespace acaia {

using BdAddress = std::array<uint8_t, 6>; // most significant byte first, as printed
using SystemId = std::array<uint8_t, 8>;   // little-endian, as sent over the air

/**
 * System ID derived from a public device address, as the Bluetooth DIS spec
 * suggests: 40-bit manufacturer identifier = 0xFFFE inserted after the lower
 * 24 address bits, followed by the 24-bit OUI.
 * 28:37:2F:80:E3:82 -> 82 E3 80 FE FF 2F 37 28.
 */
SystemId systemIdFromAddress(const BdAddress& address);

} // namespace acaia
