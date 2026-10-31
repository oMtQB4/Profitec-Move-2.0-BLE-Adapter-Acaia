#include "scale/drivers/WeighMaster.hpp"

#include "scale/Bytes.hpp"
#include "scale/NameMatch.hpp"

namespace scale {

namespace {

constexpr GattProfile kProfile{"fff0", "fff4", "fff1", WriteMode::WithoutResponse};

constexpr uint8_t kNegative = 0x10;
constexpr std::array<uint8_t, 1> kTare{0x02};
constexpr std::array<uint8_t, 2> kBeep{0x05, 0x00};

} // namespace

bool WeighMaster::matches(const Advertisement& advertisement)
{
    return containsIgnoreCase(advertisement.name, "weighmaster") || containsIgnoreCase(advertisement.name, "mantabrew");
}

std::optional<Reading> WeighMaster::decode(std::span<const uint8_t> data)
{
    if (data.size() < 7 || data[0] != 0x01 || data[1] != 0x02) {
        return std::nullopt;
    }
    const auto weight = static_cast<int32_t>(bytes::be24(data, 4));
    return Reading{(data[3] & kNegative) ? -weight : weight};
}

WeighMaster::TimerFrame WeighMaster::timerFrame(uint8_t action)
{
    return {0x52, 0x0B, action, 0x00, 0x00, 0x00, 0x00};
}

const GattProfile& WeighMaster::profile() const
{
    return kProfile;
}

bool WeighMaster::send(Command command, Transport& transport)
{
    switch (command) {
    case Command::Tare:
        schedule(300, kBeep); // the ECS-02 typically needs it after a tare
        return transport.write(kTare);
    case Command::TimerStart:
        return transport.write(timerFrame(0x01));
    case Command::TimerStop:
        return transport.write(timerFrame(0x00));
    case Command::TimerReset:
        return transport.write(timerFrame(0x02));
    }
    return false;
}

} // namespace scale
