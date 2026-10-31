#include "scale/drivers/DiyScales.hpp"

#include "scale/Bytes.hpp"
#include "scale/NameMatch.hpp"

namespace scale {

namespace {

constexpr GattProfile kPythonProfile{"1815", "2a59", nullptr};
constexpr GattProfile kRustProfile{"181d", "2a9d", nullptr};

} // namespace

bool DiyPythonScale::matches(const Advertisement& advertisement)
{
    return containsIgnoreCase(advertisement.name, "mpy-coffee");
}

std::optional<Reading> DiyPythonScale::decode(std::span<const uint8_t> data)
{
    if (data.size() < 2) {
        return std::nullopt;
    }
    return Reading{fromCentigrams(static_cast<int16_t>(bytes::be16(data, 0)))};
}

const GattProfile& DiyPythonScale::profile() const
{
    return kPythonProfile;
}

bool DiyRustScale::matches(const Advertisement& advertisement)
{
    // Exact name: "coffee-scale" as a substring is too generic.
    return advertisement.name.size() == 12 && startsWithIgnoreCase(advertisement.name, "coffee-scale");
}

std::optional<Reading> DiyRustScale::decode(std::span<const uint8_t> data)
{
    if (data.size() < 4) {
        return std::nullopt;
    }
    return Reading{fromCentigrams(static_cast<int32_t>(bytes::be32(data, 0)))};
}

const GattProfile& DiyRustScale::profile() const
{
    return kRustProfile;
}

} // namespace scale
