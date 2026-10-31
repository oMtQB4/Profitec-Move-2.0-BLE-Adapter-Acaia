#include "scale/drivers/WeighMyBru.hpp"

#include <cmath>

#include "scale/Bytes.hpp"
#include "scale/NameMatch.hpp"

namespace scale {

namespace {

constexpr GattProfile kProfile{"6e400001-b5a3-f393-e0a9-e50e24dcca9e", "6e400004-b5a3-f393-e0a9-e50e24dcca9e",
                               "6e400003-b5a3-f393-e0a9-e50e24dcca9e", WriteMode::Auto};

WeighMyBru::Frame frame(uint8_t action)
{
    return {0x03, 0x0A, action, 0x01, 0x00};
}

} // namespace

bool WeighMyBru::matches(const Advertisement& advertisement)
{
    // Match by name: the service UUID is the generic Nordic UART one.
    return startsWithIgnoreCase(advertisement.name, "WeighMyBru");
}

std::optional<Reading> WeighMyBru::decode(std::span<const uint8_t> data)
{
    if (data.size() < 4) {
        return std::nullopt;
    }
    const float grams = bytes::leFloat(data, 0);
    if (!std::isfinite(grams)) {
        return std::nullopt;
    }
    return Reading{fromGrams(grams)};
}

std::optional<WeighMyBru::Frame> WeighMyBru::encode(Command command)
{
    switch (command) {
    case Command::Tare:
        return frame(0x01);
    case Command::TimerStart:
        return frame(0x02);
    case Command::TimerStop:
        return frame(0x03);
    case Command::TimerReset:
        return frame(0x04);
    }
    return std::nullopt;
}

const GattProfile& WeighMyBru::profile() const
{
    return kProfile;
}

bool WeighMyBru::send(Command command, Transport& transport)
{
    const auto bytes = encode(command);
    return bytes && transport.write(*bytes);
}

} // namespace scale
