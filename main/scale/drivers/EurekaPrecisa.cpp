#include "scale/drivers/EurekaPrecisa.hpp"

namespace scale {

namespace {

constexpr GattProfile kProfile{"fff0", "fff1", "fff2", WriteMode::WithoutResponse};

constexpr std::string_view kAdvertisedNames[] = {"CFS-9002", "LSJ-001"};

constexpr uint8_t kTare = 0x31;
constexpr uint8_t kTimerStart = 0x33;
constexpr uint8_t kTimerStop = 0x34;
constexpr uint8_t kTimerReset = 0x35;

} // namespace

bool EurekaPrecisa::matches(const Advertisement& advertisement)
{
    for (auto prefix : kAdvertisedNames) {
        if (advertisement.name.starts_with(prefix)) {
            return true;
        }
    }
    return false;
}

std::optional<Reading> EurekaPrecisa::decode(std::span<const uint8_t> data)
{
    if (data.size() < 9 || data[0] != 0xAA || data[1] != 0x09 || data[2] != 0x41) {
        return std::nullopt;
    }
    const auto weight = static_cast<int32_t>(data[7] | (data[8] << 8));
    return Reading{data[6] != 0 ? -weight : weight};
}

std::optional<EurekaPrecisa::CommandBytes> EurekaPrecisa::encode(Command command)
{
    uint8_t code = 0;
    switch (command) {
    case Command::Tare:
        code = kTare;
        break;
    case Command::TimerStart:
        code = kTimerStart;
        break;
    case Command::TimerStop:
        code = kTimerStop;
        break;
    case Command::TimerReset:
        code = kTimerReset;
        break;
    default:
        return std::nullopt;
    }
    return CommandBytes{0xAA, 0x02, code, code};
}

EurekaPrecisa::CommandBytes EurekaPrecisa::encodeUnitGrams()
{
    return {0xAA, 0x03, 0x36, 0x00};
}

const GattProfile& EurekaPrecisa::profile() const
{
    return kProfile;
}

void EurekaPrecisa::onConnected(Transport& transport)
{
    transport.write(encodeUnitGrams());
}

bool EurekaPrecisa::send(Command command, Transport& transport)
{
    auto bytes = encode(command);
    return bytes && transport.write(*bytes);
}

} // namespace scale
