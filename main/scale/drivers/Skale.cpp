#include "scale/drivers/Skale.hpp"

#include <array>

#include "scale/Bytes.hpp"
#include "scale/NameMatch.hpp"

namespace scale {

namespace {

constexpr GattProfile kProfile{"ff08", "ef81", "ef80", WriteMode::Auto};

constexpr std::array<uint8_t, 1> kScreenOn{0xED};
constexpr std::array<uint8_t, 1> kShowWeight{0xEC};
constexpr std::array<uint8_t, 1> kGrams{0x03};

} // namespace

bool Skale::matches(const Advertisement& advertisement)
{
    return startsWithIgnoreCase(advertisement.name, "skale");
}

std::optional<Reading> Skale::decode(std::span<const uint8_t> data)
{
    if (data.size() < 3) {
        return std::nullopt;
    }
    return Reading{static_cast<int16_t>(bytes::le16(data, 1))};
}

std::optional<uint8_t> Skale::encode(Command command)
{
    switch (command) {
    case Command::Tare:
        return 0x10;
    case Command::TimerStart:
        return 0xDD;
    case Command::TimerStop:
        return 0xD1;
    case Command::TimerReset:
        return 0xD0;
    }
    return std::nullopt;
}

const GattProfile& Skale::profile() const
{
    return kProfile;
}

void Skale::onConnected(Transport& transport)
{
    transport.write(kScreenOn);
    schedule(100, kShowWeight);
    schedule(200, kGrams);
}

bool Skale::send(Command command, Transport& transport)
{
    const auto byte = encode(command);
    if (!byte) {
        return false;
    }
    const std::span<const uint8_t> bytes(&*byte, 1);
    return command == Command::Tare ? sendTwice(transport, bytes) : transport.write(bytes);
}

} // namespace scale
