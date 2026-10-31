#include "scale/drivers/HiroiaJimmy.hpp"

#include <array>
#include <cmath>

#include "scale/Bytes.hpp"
#include "scale/NameMatch.hpp"

namespace scale {

namespace {

constexpr GattProfile kProfile{"06c31822-8682-4744-9211-febc93e3bece", "06c31824-8682-4744-9211-febc93e3bece",
                               "06c31823-8682-4744-9211-febc93e3bece", WriteMode::Auto};

constexpr uint8_t kOunceModeOffset = 8;
constexpr double kGramsPerOunce = 28.349523125;
constexpr std::array<uint8_t, 2> kTare{0x07, 0x00};

} // namespace

bool HiroiaJimmy::matches(const Advertisement& advertisement)
{
    return startsWithIgnoreCase(advertisement.name, "hiroia");
}

std::optional<Reading> HiroiaJimmy::decode(std::span<const uint8_t> data)
{
    if (data.size() < 7) {
        return std::nullopt;
    }
    auto raw = static_cast<int32_t>(bytes::le24(data, 4));
    if (raw & 0x800000) {
        raw -= 0x1000000; // sign-extend 24 bits
    }
    if (data[0] > kOunceModeOffset) {
        return Reading{static_cast<int32_t>(std::lround(raw / 1000.0 * kGramsPerOunce * 10.0))};
    }
    return Reading{raw};
}

const GattProfile& HiroiaJimmy::profile() const
{
    return kProfile;
}

bool HiroiaJimmy::send(Command command, Transport& transport)
{
    if (command != Command::Tare) {
        return false;
    }
    return sendTwice(transport, kTare);
}

} // namespace scale
