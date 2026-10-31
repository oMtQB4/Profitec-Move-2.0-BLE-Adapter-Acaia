#include "scale/drivers/Smartchef.hpp"

#include "scale/Bytes.hpp"
#include "scale/NameMatch.hpp"

namespace scale {

namespace {

constexpr GattProfile kProfile{"fff0", "fff1", nullptr};

} // namespace

bool Smartchef::matches(const Advertisement& advertisement)
{
    return containsIgnoreCase(advertisement.name, "smartchef");
}

std::optional<Reading> Smartchef::decode(std::span<const uint8_t> data)
{
    if (data.size() < 9) {
        return std::nullopt;
    }
    const int32_t weight = bytes::be16(data, 5);
    return Reading{data[3] > 10 ? -weight : weight};
}

const GattProfile& Smartchef::profile() const
{
    return kProfile;
}

} // namespace scale
