#include "scale/drivers/Blackcoffee.hpp"

#include "scale/Bytes.hpp"
#include "scale/NameMatch.hpp"

namespace scale {

namespace {

constexpr GattProfile kProfile{"ffb0", "ffb2", nullptr};

} // namespace

bool Blackcoffee::matches(const Advertisement& advertisement)
{
    return containsIgnoreCase(advertisement.name, "blackcoffee") || containsIgnoreCase(advertisement.name, "my_scale");
}

std::optional<Reading> Blackcoffee::decode(std::span<const uint8_t> data)
{
    if (data.size() < 15) {
        return std::nullopt;
    }
    const uint8_t signNibble = data[2] >> 4;
    const auto milligrams = static_cast<int32_t>((data[3] & 0x0F) << 24 | bytes::be24(data, 4));
    const bool negative = signNibble == 0x8 || signNibble == 0xC;
    return Reading{fromMilligrams(negative ? -milligrams : milligrams)};
}

const GattProfile& Blackcoffee::profile() const
{
    return kProfile;
}

} // namespace scale
